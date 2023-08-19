// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_lacros.h"

#include "base/test/bind.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_content_manager_test_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const std::u16string kAppId = u"app_id";
constexpr char kScreenShareLabel[] = "label";

// TODO(crbug.com/1291609): Mock Lacros side of crosapi instead when possible.
// This will allow these tests to be just unit_tests, not
// lacros_chrome_browsertests.
class MockDlpCrosapi : public crosapi::mojom::Dlp {
 public:
  MOCK_METHOD(void,
              DlpRestrictionsUpdated,
              (const std::string&, crosapi::mojom::DlpRestrictionSetPtr),
              (override));
  MOCK_METHOD(void,
              CheckScreenShareRestriction,
              (crosapi::mojom::ScreenShareAreaPtr,
               const std::u16string&,
               CheckScreenShareRestrictionCallback),
              (override));
  MOCK_METHOD(void,
              OnScreenShareStarted,
              (const std::string&,
               crosapi::mojom::ScreenShareAreaPtr,
               const ::std::u16string&,
               ::mojo::PendingRemote<crosapi::mojom::StateChangeDelegate>),
              (override));
  MOCK_METHOD(void,
              OnScreenShareStopped,
              (const std::string&, crosapi::mojom::ScreenShareAreaPtr),
              (override));
};

}  // namespace

class DlpContentManagerLacrosBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // If DLP interface is not available on this version of ash-chrome, this
    // test suite will no-op.
    if (!IsServiceAvailable())
      return;

    test_helper_ = std::make_unique<DlpContentManagerTestHelper>();

    // Replace the production DLP service with a mock for testing.
    mojo::Remote<crosapi::mojom::Dlp>& remote =
        chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Dlp>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

  // Returns whether the DLP interface is available. It may
  // not be available on earlier versions of ash-chrome.
  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service && lacros_service->IsAvailable<crosapi::mojom::Dlp>();
  }

  void SetDlpInterfaceVersion(int version) {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->interface_versions.value()[crosapi::mojom::Dlp::Uuid_] =
        version;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  DlpContentManagerLacros* manager() {
    return static_cast<DlpContentManagerLacros*>(
        test_helper_->GetContentManager());
  }

  testing::StrictMock<MockDlpCrosapi>& service() { return service_; }

 private:
  testing::StrictMock<MockDlpCrosapi> service_;
  mojo::Receiver<crosapi::mojom::Dlp> receiver_{&service_};
  std::unique_ptr<DlpContentManagerTestHelper> test_helper_;
};

IN_PROC_BROWSER_TEST_F(
    DlpContentManagerLacrosBrowserTest,
    CheckScreenShareRestrictionFullScreenNotSupportedVersion) {
  // If DLP interface is not available on this version of ash-chrome, this test
  // suite will no-op.
  if (!IsServiceAvailable())
    return;

  SetDlpInterfaceVersion(0);

  // No call to mojo remote should happen in this case (ensured by StrictMock).
  // The request is just silently allowed.
  base::RunLoop run_loop;
  manager()->CheckScreenShareRestriction(
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                              content::DesktopMediaID::kFakeId),
      kAppId, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_TRUE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerLacrosBrowserTest,
                       CheckScreenShareRestrictionFullScreenAllowed) {
  // If DLP interface is not available on this version of ash-chrome, this test
  // suite will no-op.
  if (!IsServiceAvailable())
    return;

  SetDlpInterfaceVersion(service().Version_);

  EXPECT_CALL(service(), CheckScreenShareRestriction)
      .WillOnce([](crosapi::mojom::ScreenShareAreaPtr area,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_FALSE(area->window_id.has_value());
        EXPECT_EQ(kAppId, application_title);
        std::move(callback).Run(/*should_proceed=*/true);
      });

  base::RunLoop run_loop;
  manager()->CheckScreenShareRestriction(
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                              content::DesktopMediaID::kFakeId),
      kAppId, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_TRUE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerLacrosBrowserTest,
                       CheckScreenShareRestrictionFullScreenNotAllowed) {
  // If DLP interface is not available on this version of ash-chrome, this test
  // suite will no-op.
  if (!IsServiceAvailable())
    return;

  SetDlpInterfaceVersion(service().Version_);

  EXPECT_CALL(service(), CheckScreenShareRestriction)
      .WillOnce([](crosapi::mojom::ScreenShareAreaPtr area,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_FALSE(area->window_id.has_value());
        EXPECT_EQ(kAppId, application_title);
        std::move(callback).Run(/*should_proceed=*/false);
      });

  base::RunLoop run_loop;
  manager()->CheckScreenShareRestriction(
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                              content::DesktopMediaID::kFakeId),
      kAppId, base::BindLambdaForTesting([&](bool allowed) {
        EXPECT_FALSE(allowed);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerLacrosBrowserTest,
                       ScreenShareStartedStop) {
  // If DLP interface is not available on this version of ash-chrome, this test
  // suite will no-op.
  if (!IsServiceAvailable())
    return;

  SetDlpInterfaceVersion(service().Version_);

  // Setup callback on remote service.
  base::RunLoop bound_loop;
  ::mojo::PendingRemote<crosapi::mojom::StateChangeDelegate> pending_delegate;
  EXPECT_CALL(service(), OnScreenShareStarted)
      .WillOnce([&](const std::string& label,
                    crosapi::mojom::ScreenShareAreaPtr area,
                    const std::u16string& application_title,
                    ::mojo::PendingRemote<crosapi::mojom::StateChangeDelegate>
                        delegate) {
        EXPECT_EQ(kScreenShareLabel, label);
        EXPECT_FALSE(area->window_id.has_value());
        EXPECT_EQ(kAppId, application_title);
        pending_delegate = std::move(delegate);
        bound_loop.Quit();
      });

  // Call DLP manager and expect stop callback.
  base::RunLoop stopped_run_loop;
  manager()->OnScreenShareStarted(
      kScreenShareLabel,
      {content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               content::DesktopMediaID::kFakeId)},
      kAppId, base::BindLambdaForTesting([&]() { stopped_run_loop.Quit(); }),
      /*state_change_callback=*/base::DoNothing(),
      /*source_callback=*/base::DoNothing());

  // Bind remote delegate.
  bound_loop.Run();
  mojo::Remote<crosapi::mojom::StateChangeDelegate> remote_delegate(
      std::move(pending_delegate));
  EXPECT_TRUE(remote_delegate);

  // Request remote delegate to stop.
  remote_delegate->OnStop();

  // Wait for stop callback.
  stopped_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerLacrosBrowserTest,
                       ScreenShareStartedPauseAndResume) {
  // If DLP interface is not available on this version of ash-chrome, this test
  // suite will no-op.
  if (!IsServiceAvailable())
    return;

  SetDlpInterfaceVersion(service().Version_);

  // Setup callback on remote service.
  base::RunLoop bound_loop;
  ::mojo::PendingRemote<crosapi::mojom::StateChangeDelegate> pending_delegate;
  EXPECT_CALL(service(), OnScreenShareStarted)
      .WillOnce([&](const std::string& label,
                    crosapi::mojom::ScreenShareAreaPtr area,
                    const std::u16string& application_title,
                    ::mojo::PendingRemote<crosapi::mojom::StateChangeDelegate>
                        delegate) {
        EXPECT_EQ(kScreenShareLabel, label);
        EXPECT_FALSE(area->window_id.has_value());
        EXPECT_EQ(kAppId, application_title);
        pending_delegate = std::move(delegate);
        bound_loop.Quit();
      });

  // Call DLP manager and expect state change callbacks.
  base::RunLoop paused_run_loop;
  base::RunLoop resumed_run_loop;
  content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                   content::DesktopMediaID::kFakeId);
  manager()->OnScreenShareStarted(
      kScreenShareLabel, {media_id}, kAppId,
      /*stop_callback=*/base::DoNothing(),
      base::BindLambdaForTesting(
          [&](const content::DesktopMediaID& in_media_id,
              blink::mojom::MediaStreamStateChange new_state) {
            EXPECT_EQ(media_id, in_media_id);
            if (new_state == blink::mojom::MediaStreamStateChange::PAUSE) {
              paused_run_loop.Quit();
            } else if (new_state ==
                       blink::mojom::MediaStreamStateChange::PLAY) {
              resumed_run_loop.Quit();
            } else {
              NOTREACHED();
            }
          }),
      /*source_callback=*/base::DoNothing());

  // Bind remote delegate.
  bound_loop.Run();
  mojo::Remote<crosapi::mojom::StateChangeDelegate> remote_delegate(
      std::move(pending_delegate));
  EXPECT_TRUE(remote_delegate);

  // Request remote delegate to pause.
  remote_delegate->OnPause();

  // Wait for stop callback.
  paused_run_loop.Run();

  // Request remote delegate to resume.
  remote_delegate->OnResume();

  // Wait for stop callback.
  resumed_run_loop.Run();
}

}  // namespace policy
