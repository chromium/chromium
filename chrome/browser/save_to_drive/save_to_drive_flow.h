// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_user_data.h"

struct AccountInfo;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions::api::pdf_viewer_private {
struct SaveToDriveProgress;
}  // namespace extensions::api::pdf_viewer_private

namespace save_to_drive {

class ContentReader;
class DriveUploader;
class SaveToDriveEventDispatcher;

// Invoked when the account chooser is closed with the account info if an
// account is chosen, or `std::nullopt` if the account chooser is canceled.
using AccountChooserCallback =
    base::OnceCallback<void(std::optional<AccountInfo>)>;

// This class is responsible for orchastrating the entire save to Drive flow
// on the browser process from showing the account chooser, reading the file
// size, and initiating the appropriate drive uploader to save the file to
// Drive. This flow will be tied to the lifetime of the document. It is
// responsible for cleaning up its resources when the flow is stopped.
// This flow should only be called on the UI thread.
class SaveToDriveFlow : public content::DocumentUserData<SaveToDriveFlow> {
 public:
  SaveToDriveFlow(const SaveToDriveFlow&) = delete;
  SaveToDriveFlow& operator=(const SaveToDriveFlow&) = delete;
  ~SaveToDriveFlow() override;

  // Starts the save to Drive flow.
  void Run();

  // Cleans up the flow and its resources. This is called when the flow is
  // aborted or completed.
  void Stop();

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
    // Simulates the account chooser being closed with an `AccountInfo`.
    // `std::nullopt` means the account chooser was canceled. It only simulates
    // the action for the first call to the account chooser following it.
    void SimulateAccountChooserAction(std::optional<AccountInfo> account_info);
    // It will never return `nullptr` before the `flow_` runs.
    content::RenderFrameHost* rfh();

   private:
    base::WeakPtr<SaveToDriveFlow> flow_;
  };

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

  SaveToDriveFlow(content::RenderFrameHost* render_frame_host,
                  std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher,
                  std::unique_ptr<ContentReader> content_reader);

  void ShowAccountChooser(AccountChooserCallback callback);
  void OnAccountChosen(std::optional<AccountInfo> account_info);
  void OnOpenContent(AccountInfo account_info, bool success);
  void OnUploadProgress(
      extensions::api::pdf_viewer_private::SaveToDriveProgress progress);

  std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher_;
  std::unique_ptr<ContentReader> content_reader_;
  std::unique_ptr<DriveUploader> drive_uploader_;

  // Used for testing to simulate the account chooser being closed with an
  // account info. `nullptr` means it was not set, while `std::nullopt` means
  // the account chooser was canceled.
  std::unique_ptr<std::optional<AccountInfo>> account_info_for_testing_;

  // This is set when an account is chosen.
  std::optional<SaveToDriveAccountInfo> save_to_drive_account_info_;

  base::WeakPtrFactory<SaveToDriveFlow> weak_ptr_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_SAVE_TO_DRIVE_FLOW_H_
