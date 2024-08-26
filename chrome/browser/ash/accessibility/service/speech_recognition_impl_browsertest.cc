// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/speech_recognition_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_recognizer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "services/accessibility/public/mojom/assistive_technology_type.mojom.h"
#include "services/accessibility/public/mojom/speech_recognition.mojom.h"

namespace {

// A mock implementation of the SpeechRecognitionEventObserver interface. When
// speech recognition starts, we can bind the resulting pending receiver to
// this implementation, allowing us to know when speech recognition events are
// dispatched.
class MockSpeechRecognitionEventObserverImpl
    : public ax::mojom::SpeechRecognitionEventObserver {
 public:
  MockSpeechRecognitionEventObserverImpl(
      mojo::PendingReceiver<ax::mojom::SpeechRecognitionEventObserver>
          pending_receiver,
      base::RepeatingCallback<void()> on_stop_callback,
      base::RepeatingCallback<void(ax::mojom::SpeechRecognitionResultEventPtr)>
          on_result_callback,
      base::RepeatingCallback<void(ax::mojom::SpeechRecognitionErrorEventPtr)>
          on_error_callback)
      : receiver_(this, std::move(pending_receiver)),
        on_stop_callback_(std::move(on_stop_callback)),
        on_result_callback_(std::move(on_result_callback)),
        on_error_callback_(std::move(on_error_callback)) {}
  MockSpeechRecognitionEventObserverImpl(
      const MockSpeechRecognitionEventObserverImpl&) = delete;
  MockSpeechRecognitionEventObserverImpl& operator=(
      const MockSpeechRecognitionEventObserverImpl&) = delete;
  ~MockSpeechRecognitionEventObserverImpl() override {}

  void OnStop() override { on_stop_callback_.Run(); }
  void OnResult(ax::mojom::SpeechRecognitionResultEventPtr event) override {
    on_result_callback_.Run(std::move(event));
  }
  void OnError(ax::mojom::SpeechRecognitionErrorEventPtr event) override {
    on_error_callback_.Run(std::move(event));
  }

 private:
  mojo::Receiver<ax::mojom::SpeechRecognitionEventObserver> receiver_;
  base::RepeatingCallback<void()> on_stop_callback_;
  base::RepeatingCallback<void(ax::mojom::SpeechRecognitionResultEventPtr)>
      on_result_callback_;
  base::RepeatingCallback<void(ax::mojom::SpeechRecognitionErrorEventPtr)>
      on_error_callback_;
};
}  // namespace

namespace ash {

// Tests for SpeechRecognitionImpl - this file mostly tests that the class
// correctly manages its internal state and can dispatch events to the proper
// event observers.
class SpeechRecognitionImplTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    sr_test_helper_ = std::make_unique<SpeechRecognitionTestHelper>(
        speech::SpeechRecognitionType::kNetwork,
        media::mojom::RecognizerClientType::kDictation);
    sr_test_helper_->SetUp(browser()->profile());
    sr_impl_ = std::make_unique<SpeechRecognitionImpl>(browser()->profile());
  }

  void TearDownOnMainThread() override { sr_impl_.reset(); }

  void HandleSpeechRecognitionStopped(const std::string& key) {
    sr_impl_->HandleSpeechRecognitionStopped(key);
  }

  void HandleSpeechRecognitionResult(const std::string& key,
                                     const std::u16string& transcript,
                                     bool is_final) {
    sr_impl_->HandleSpeechRecognitionResult(key, transcript, is_final);
  }

  void HandleSpeechRecognitionError(const std::string& key,
                                    const std::string& error) {
    sr_impl_->HandleSpeechRecognitionError(key, error);
  }

  extensions::SpeechRecognitionPrivateRecognizer* GetSpeechRecognizer(
      const std::string& key) {
    return sr_impl_->GetSpeechRecognizer(key);
  }

  void CreateEventObserverWrapper(const std::string& key) {
    sr_impl_->CreateEventObserverWrapper(key);
  }

  SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper*
  GetEventObserverWrapper(const std::string& key) {
    return sr_impl_->GetEventObserverWrapper(key);
  }

  std::string CreateKey(ax::mojom::AssistiveTechnologyType type) {
    return sr_impl_->CreateKey(type);
  }

  int GetNumRecognizers() { return sr_impl_->recognizers_.size(); }
  int GetNumEventObserverWrappers() {
    return sr_impl_->event_observer_wrappers_.size();
  }

  std::unique_ptr<SpeechRecognitionTestHelper> sr_test_helper_;
  std::unique_ptr<SpeechRecognitionImpl> sr_impl_;
};

IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, GetSpeechRecognizer) {
  extensions::SpeechRecognitionPrivateRecognizer* first = nullptr;
  extensions::SpeechRecognitionPrivateRecognizer* second = nullptr;
  first = GetSpeechRecognizer("Hello");
  second = GetSpeechRecognizer("Hello");
  ASSERT_NE(nullptr, first);
  ASSERT_NE(nullptr, second);
  ASSERT_EQ(first, second);
  second = GetSpeechRecognizer("World");
  ASSERT_NE(nullptr, second);
  ASSERT_NE(first, second);
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, GetEventObserverWrapper) {
  SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper* first = nullptr;
  SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper* second =
      nullptr;
  ASSERT_EQ(nullptr, GetEventObserverWrapper("Hello"));
  CreateEventObserverWrapper("Hello");
  first = GetEventObserverWrapper("Hello");
  second = GetEventObserverWrapper("Hello");
  ASSERT_NE(nullptr, first);
  ASSERT_NE(nullptr, second);
  ASSERT_EQ(first, second);
  CreateEventObserverWrapper("World");
  second = GetEventObserverWrapper("World");
  ASSERT_NE(nullptr, second);
  ASSERT_NE(first, second);
}

// Verifies that speech recognition can be started and stopped, and that the
// correct number of recognizers and observers are created.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, StartAndStop) {
  ASSERT_EQ(0, GetNumRecognizers());
  ASSERT_EQ(0, GetNumEventObserverWrappers());

  auto start_options = ax::mojom::StartOptions::New();
  start_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  sr_impl_->Start(std::move(start_options), base::DoNothing());
  sr_test_helper_->WaitForRecognitionStarted();
  ASSERT_EQ(1, GetNumRecognizers());
  ASSERT_EQ(1, GetNumEventObserverWrappers());

  auto stop_options = ax::mojom::StopOptions::New();
  stop_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  sr_impl_->Stop(std::move(stop_options), base::DoNothing());
  sr_test_helper_->WaitForRecognitionStopped();
  // Note that recognizers are kept alive because they can be re-used when
  // starting a new session of speech recognition. Event observer wrappers
  // are only valid during a session of speech recognition and should be
  // recreated when a new session starts.
  ASSERT_EQ(1, GetNumRecognizers());
  ASSERT_EQ(0, GetNumEventObserverWrappers());
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, StartAndStopWithClientId) {
  auto start_options = ax::mojom::StartOptions::New();
  start_options->type = ax::mojom::AssistiveTechnologyType::kDictation;

  sr_impl_->Start(std::move(start_options), base::DoNothing());
  sr_test_helper_->WaitForRecognitionStarted();
  ASSERT_EQ(1, GetNumRecognizers());
  ASSERT_EQ(1, GetNumEventObserverWrappers());

  auto stop_options = ax::mojom::StopOptions::New();
  stop_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  sr_impl_->Stop(std::move(stop_options), base::DoNothing());
  sr_test_helper_->WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, StartOptions) {
  base::RunLoop waiter;
  auto options = ax::mojom::StartOptions::New();

  ax::mojom::AssistiveTechnologyType type =
      ax::mojom::AssistiveTechnologyType::kDictation;
  std::string key = CreateKey(type);
  options->type = type;
  options->locale = "ja-JP";
  options->interim_results = true;
  base::RepeatingCallback<void(ax::mojom::SpeechRecognitionStartInfoPtr info)>
      callback = base::BindLambdaForTesting(
          [&waiter, &key, this](ax::mojom::SpeechRecognitionStartInfoPtr info) {
            auto* recognizer = GetSpeechRecognizer(key);
            ASSERT_EQ("ja-JP", recognizer->locale());
            ASSERT_EQ(true, recognizer->interim_results());
            ASSERT_TRUE(info->observer_or_error->is_observer());
            ASSERT_FALSE(info->observer_or_error->is_error());
            waiter.Quit();
          });
  sr_impl_->Start(std::move(options), std::move(callback));
  waiter.Run();
}

// Verifies that SpeechRecognitionImpl notifies the correct event observer.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, DispatchStopEvent) {
  base::RunLoop waiter;
  // Called when a speech recognition stop event comes through.
  base::RepeatingCallback<void()> on_stop_callback =
      base::BindLambdaForTesting([&waiter]() { waiter.Quit(); });

  std::unique_ptr<MockSpeechRecognitionEventObserverImpl>
      mock_event_observer_impl;

  auto start_options = ax::mojom::StartOptions::New();
  ax::mojom::AssistiveTechnologyType type =
      ax::mojom::AssistiveTechnologyType::kDictation;
  std::string key = CreateKey(type);
  start_options->type = type;

  sr_impl_->Start(
      std::move(start_options),
      base::BindLambdaForTesting(
          [&mock_event_observer_impl, &on_stop_callback, &key,
           this](ax::mojom::SpeechRecognitionStartInfoPtr info) {
            ASSERT_EQ(1, GetNumEventObserverWrappers());
            mock_event_observer_impl =
                std::make_unique<MockSpeechRecognitionEventObserverImpl>(
                    std::move(info->observer_or_error->get_observer()),
                    std::move(on_stop_callback), base::DoNothing(),
                    base::DoNothing());
            // Calling this method will dispatch a request to
            // `mock_event_observer_impl`.
            HandleSpeechRecognitionStopped(key);
          }));
  sr_test_helper_->WaitForRecognitionStarted();
  waiter.Run();
  ASSERT_EQ(0, GetNumEventObserverWrappers());
}

// Verifies that speech recognition results can be returned.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, DispatchResultEvent) {
  // Variables used throughout the test.
  base::RunLoop waiter;
  ax::mojom::AssistiveTechnologyType type =
      ax::mojom::AssistiveTechnologyType::kDictation;
  std::string key = CreateKey(type);

  // Called after speech recognition has been stopped.
  base::RepeatingCallback<void()> on_stop_callback =
      base::BindLambdaForTesting([&waiter]() { waiter.Quit(); });

  // Called when a speech recognition result has been returned.
  base::RepeatingCallback<void(
      ax::mojom::SpeechRecognitionResultEventPtr event)>
      on_result_callback = base::BindLambdaForTesting(
          [this, &key](ax::mojom::SpeechRecognitionResultEventPtr event) {
            ASSERT_EQ("Hello world", event->transcript);
            ASSERT_EQ(true, event->is_final);
            HandleSpeechRecognitionStopped(key);
          });

  std::unique_ptr<MockSpeechRecognitionEventObserverImpl>
      mock_event_observer_impl;

  auto start_options = ax::mojom::StartOptions::New();
  start_options->type = type;
  sr_impl_->Start(
      std::move(start_options),
      base::BindLambdaForTesting(
          [&mock_event_observer_impl, &on_stop_callback, &on_result_callback,
           &key, this](ax::mojom::SpeechRecognitionStartInfoPtr info) {
            mock_event_observer_impl =
                std::make_unique<MockSpeechRecognitionEventObserverImpl>(
                    std::move(info->observer_or_error->get_observer()),
                    std::move(on_stop_callback), std::move(on_result_callback),
                    base::DoNothing());
            // Calling this method will dispatch a request to
            // `mock_event_observer_impl`.
            HandleSpeechRecognitionResult(
                /*key=*/key, /*transcript=*/u"Hello world", /*is_final=*/true);
          }));
  waiter.Run();
}

// Verifies that SpeechRecognitionImpl notifies the correct event observer
// when dispatching error events.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, DispatchErrorEvent) {
  // Variables used throughout the test.
  base::RunLoop waiter;
  ax::mojom::AssistiveTechnologyType type =
      ax::mojom::AssistiveTechnologyType::kDictation;
  std::string key = CreateKey(type);

  // Called when a speech recognition result has been returned.
  base::RepeatingCallback<void(ax::mojom::SpeechRecognitionErrorEventPtr event)>
      on_error_callback = base::BindLambdaForTesting(
          [&waiter](ax::mojom::SpeechRecognitionErrorEventPtr event) {
            ASSERT_EQ("Hello world", event->message);
            waiter.Quit();
          });

  std::unique_ptr<MockSpeechRecognitionEventObserverImpl>
      mock_event_observer_impl;

  auto start_options = ax::mojom::StartOptions::New();
  start_options->type = type;
  sr_impl_->Start(
      std::move(start_options),
      base::BindLambdaForTesting(
          [&mock_event_observer_impl, &on_error_callback, &key,
           this](ax::mojom::SpeechRecognitionStartInfoPtr info) {
            ASSERT_EQ(1, GetNumEventObserverWrappers());
            mock_event_observer_impl =
                std::make_unique<MockSpeechRecognitionEventObserverImpl>(
                    std::move(info->observer_or_error->get_observer()),
                    base::DoNothing(), base::DoNothing(),
                    std::move(on_error_callback));
            // Calling this method will dispatch a request to
            // `mock_event_observer_impl`.
            HandleSpeechRecognitionError(
                /*key=*/key, /*error=*/"Hello world");
          }));
  waiter.Run();
  ASSERT_EQ(0, GetNumEventObserverWrappers());
}

// Triggers an error by attempting to start speech recognition twice and
// verifies that the correct error message is returned.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, StartError) {
  auto first_start_options = ax::mojom::StartOptions::New();
  first_start_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  sr_impl_->Start(std::move(first_start_options), base::DoNothing());
  sr_test_helper_->WaitForRecognitionStarted();

  base::RunLoop waiter;
  auto second_start_options = ax::mojom::StartOptions::New();
  second_start_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  sr_impl_->Start(std::move(second_start_options),
                  base::BindLambdaForTesting(
                      [&waiter](ax::mojom::SpeechRecognitionStartInfoPtr info) {
                        ASSERT_EQ("Speech recognition already started",
                                  info->observer_or_error->get_error());
                        ASSERT_FALSE(info->observer_or_error->is_observer());
                        waiter.Quit();
                      }));
  waiter.Run();
}

// Triggers an error by attempting to stop speech recognition when it is already
// stopped and verifies that the correct error message is returned.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionImplTest, StopError) {
  auto stop_options = ax::mojom::StopOptions::New();
  stop_options->type = ax::mojom::AssistiveTechnologyType::kDictation;

  base::RunLoop waiter;
  sr_impl_->Stop(std::move(stop_options),
                 base::BindLambdaForTesting(
                     [&waiter](const std::optional<std::string>& error) {
                       ASSERT_EQ("Speech recognition already stopped",
                                 error.value());
                       waiter.Quit();
                     }));
  waiter.Run();
}

}  // namespace ash
