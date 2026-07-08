// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace extensions {

namespace {

class ExtensionApiTestStreamProvider : public dictation::StreamProvider {
 public:
  ExtensionApiTestStreamProvider(
      content::BrowserContext* browser_context,
      const ExtensionId& extension_id,
      dictation::DictationMultiplexer::StreamId stream_id)
      : browser_context_(browser_context),
        extension_id_(extension_id),
        stream_id_(stream_id) {}
  ~ExtensionApiTestStreamProvider() override = default;

  // StreamProvider:
  void BindToTargetAndConnect(
      std::unique_ptr<dictation::Target> target) override {
    target_ = std::move(target);
    api::dictation_private::StartStreamDetails details;
    details.stream_id = stream_id_.value();
    api::dictation_private::DictationContext context;
    context.annotated_page_content = std::vector<uint8_t>{1, 2, 3};
    context.inner_text = "Foo Bar";
    context.editable_content = "Existing content";
    details.context = std::move(context);

    base::ListValue event_args =
        api::dictation_private::OnStartStream::Create(details);

    auto event = std::make_unique<Event>(
        events::DICTATION_PRIVATE_ON_START_STREAM,
        api::dictation_private::OnStartStream::kEventName,
        std::move(event_args), browser_context_);

    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
  }

  void Stop() override {
    api::dictation_private::EndStreamDetails details;
    details.stream_id = stream_id_.value();

    base::ListValue event_args =
        api::dictation_private::OnEndStream::Create(details);

    auto event =
        std::make_unique<Event>(events::DICTATION_PRIVATE_ON_END_STREAM,
                                api::dictation_private::OnEndStream::kEventName,
                                std::move(event_args), browser_context_);

    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
  }

  void OnTranscriptionUpdated(const std::string& data, bool is_final) override {
    transcription_updates_.push_back({data, is_final});
  }

  void OnStreamStateChanged(
      dictation::StreamProvider::StreamState state) override {
    state_changes_.push_back(state);
  }

  dictation::StreamProvider::StreamState GetState() const override {
    return state_changes_.empty()
               ? dictation::StreamProvider::StreamState::kInitializing
               : state_changes_.back();
  }

  const dictation::Target* GetTarget() const override { return target_.get(); }

  struct TranscriptionUpdate {
    std::string data;
    bool is_final;
    bool operator==(const TranscriptionUpdate&) const = default;
  };

  const std::vector<TranscriptionUpdate>& transcription_updates() const {
    return transcription_updates_;
  }
  const std::vector<dictation::StreamProvider::StreamState>& state_changes()
      const {
    return state_changes_;
  }

 private:
  std::unique_ptr<dictation::Target> target_;
  raw_ptr<content::BrowserContext> browser_context_;
  ExtensionId extension_id_;
  dictation::DictationMultiplexer::StreamId stream_id_;
  std::vector<TranscriptionUpdate> transcription_updates_;
  std::vector<dictation::StreamProvider::StreamState> state_changes_;
};

}  // namespace

class DictationPrivateApiTest : public ExtensionApiTest {
 public:
  DictationPrivateApiTest() = default;
  ~DictationPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    base::FilePath extension_path =
        test_data_dir_.AppendASCII("dictation_private").AppendASCII("allowed");
    ExtensionId extension_id =
        crx_file::id_util::GenerateIdForPath(extension_path);
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    extension_id);
  }

 private:
  base::test::ScopedFeatureList feature_list_ =
      dictation::CreateEnablingFeatureList();
};

IN_PROC_BROWSER_TEST_F(DictationPrivateApiTest, Basic) {
  ResultCatcher catcher;
  ExtensionTestMessageListener ready_listener("ready");
  ready_listener.set_failure_message("failed");

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("dictation_private/allowed");
  const Extension* extension = LoadExtension(extension_path);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  dictation::DictationKeyedService* service =
      dictation::DictationKeyedService::Get(profile());
  ASSERT_TRUE(service);
  dictation::DictationMultiplexer& multiplexer = service->multiplexer();

  const dictation::DictationMultiplexer::StreamId test_stream_id(123);
  ExtensionApiTestStreamProvider test_stream_provider(
      profile(), extension->id(), test_stream_id);
  multiplexer.RegisterStreamProvider(test_stream_id, &test_stream_provider);

  auto target = std::make_unique<dictation::Target>(
      dictation::TargetId{content::WeakDocumentPtr()});
  test_stream_provider.BindToTargetAndConnect(std::move(target));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  multiplexer.UnregisterStreamProvider(test_stream_id);

  // Assert that data from extension reached the stream provider.
  const auto& transcription_updates =
      test_stream_provider.transcription_updates();
  ASSERT_EQ(2u, transcription_updates.size());
  EXPECT_EQ(
      (ExtensionApiTestStreamProvider::TranscriptionUpdate{"Hello", false}),
      transcription_updates[0]);
  EXPECT_EQ((ExtensionApiTestStreamProvider::TranscriptionUpdate{"Hello world",
                                                                 true}),
            transcription_updates[1]);

  const auto& state_changes = test_stream_provider.state_changes();
  ASSERT_EQ(2u, state_changes.size());
  EXPECT_EQ(dictation::StreamProvider::StreamState::kTranscribing,
            state_changes[0]);
  EXPECT_EQ(dictation::StreamProvider::StreamState::kComplete,
            state_changes[1]);
}

// Test that a non-allowlisted extension cannot access the API.
IN_PROC_BROWSER_TEST_F(DictationPrivateApiTest, BlockedCannotAccessApi) {
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("dictation_private/blocked");

  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_path, {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);
  EXPECT_THAT(extension->install_warnings(),
              testing::Contains(testing::Field(
                  &extensions::InstallWarning::message,
                  testing::StartsWith("'dictationPrivate' is not allowed"))));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
