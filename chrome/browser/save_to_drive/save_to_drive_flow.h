// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "content/public/browser/document_user_data.h"

struct AccountInfo;

namespace content {
class RenderFrameHost;
}  // namespace content

class HatsService;

namespace save_to_drive {

class AccountChooser;
class ContentReader;
class DriveUploader;
class SaveToDriveEventDispatcher;

// This class is responsible for orchastrating the entire save to Drive flow
// on the browser process from showing the account chooser, reading the file
// size, and initiating the appropriate drive uploader to save the file to
// Drive. This flow will be tied to the lifetime of the document. It is
// responsible for cleaning up its resources when the flow is stopped.
// This flow should only be called on the UI thread.
class SaveToDriveFlow : public content::DocumentUserData<SaveToDriveFlow> {
 public:
  using CreateCallback = base::RepeatingCallback<SaveToDriveFlow*(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher,
      std::unique_ptr<ContentReader> content_reader,
      std::unique_ptr<AccountChooser> account_chooser,
      HatsService* hats_service)>;

  // Factory method to create a new instance of `SaveToDriveFlow`. This is
  // used to allow for creating a mock flow in tests through
  // `SetCreateCallbackForTesting`.
  static SaveToDriveFlow* Create(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher,
      std::unique_ptr<ContentReader> content_reader,
      std::unique_ptr<AccountChooser> account_chooser,
      HatsService* hats_service);

  // Sets the callback to create a new instance of `SaveToDriveFlow`. This
  // is used to create a mock flow in tests.
  static void SetCreateCallbackForTesting(CreateCallback* callback);

  SaveToDriveFlow(const SaveToDriveFlow&) = delete;
  SaveToDriveFlow& operator=(const SaveToDriveFlow&) = delete;
  ~SaveToDriveFlow() override;

  // Starts the save to Drive flow. Marked virtual for testing.
  virtual void Run();

  // Cleans up the flow and its resources. This is called when the flow is
  // aborted or completed. Marked virtual for testing.
  virtual void Stop();

  class TestApi {
   public:
    explicit TestApi(SaveToDriveFlow* flow);
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;
    ~TestApi();

    // It will never return `nullptr' before the `flow_` runs.
    const ContentReader* content_reader() const;
    const DriveUploader* drive_uploader() const;
    // It will never return `nullptr' before the `flow_` runs.
    const SaveToDriveEventDispatcher* event_dispatcher() const;
    // It will never return `nullptr` before the `flow_` runs.
    content::RenderFrameHost* rfh();

   private:
    base::WeakPtr<SaveToDriveFlow> flow_;
  };

 protected:
  SaveToDriveFlow(content::RenderFrameHost* render_frame_host,
                  std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher,
                  std::unique_ptr<ContentReader> content_reader,
                  std::unique_ptr<AccountChooser> account_chooser,
                  HatsService* hats_service);

 private:
  friend class content::DocumentUserData<SaveToDriveFlow>;
  friend class TestApi;

  struct SaveToDriveAccountInfo {
    // The email of the account that is chosen to save the PDF to Drive. It is
    // used to construct the URL to open the file in Drive or manage the quota.
    std::string email;

    // Whether the account is a managed account. It is used to determine whether
    // the account is a dasher account and show the correct manage storage URL.
    bool is_managed = false;
  };
  void OnAccountChosen(std::optional<AccountInfo> account_info);
  void OnOpenContent(AccountInfo account_info, bool success);
  void OnUploadProgress(
      extensions::api::pdf_viewer_private::SaveToDriveProgress progress);
  void ShowHatsSurveyWithDelay();
  std::string GetHatsSurveyTriggerId();

  std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher_;
  std::unique_ptr<ContentReader> content_reader_;
  std::unique_ptr<DriveUploader> drive_uploader_;
  std::unique_ptr<AccountChooser> account_chooser_;
  raw_ptr<HatsService> hats_service_ = nullptr;

  // This is set when an account is chosen.
  std::optional<SaveToDriveAccountInfo> save_to_drive_account_info_;
  // This is set after the upload starts.
  std::optional<extensions::api::pdf_viewer_private::SaveToDriveProgress>
      upload_progress_;

  base::WeakPtrFactory<SaveToDriveFlow> weak_ptr_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_
