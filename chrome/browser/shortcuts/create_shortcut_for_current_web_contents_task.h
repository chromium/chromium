// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_CREATE_SHORTCUT_FOR_CURRENT_WEB_CONTENTS_TASK_H_
#define CHROME_BROWSER_SHORTCUTS_CREATE_SHORTCUT_FOR_CURRENT_WEB_CONTENTS_TASK_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shortcuts/document_icon_fetcher_task.h"
#include "chrome/browser/shortcuts/shortcut_creator.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image_family.h"

namespace content {
class WebContents;
class RenderFrameHost;
class Page;
enum class Visibility;
}  // namespace content

namespace gfx {
class ImageSkia;
class ImageFamily;
}  // namespace gfx

namespace shortcuts {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShortcutCreationTaskResult {
  kTaskAlreadyRunning = 0,
  kInvalidRenderFrameHost = 1,
  kPageInvalidated = 2,
  kIconFetchingFailed = 3,
  kUserCancelledShortcutCreationFromDialog = 4,
  kShortcutCreationFailure = 5,
  kShortcutCreationSuccess = 6,
  kMaxValue = kShortcutCreationSuccess
};

// Used to perform the actions needed to create the shortcut on the UI thread.
// This usually involves downloading icons and badging them before passing them
// on to the OS specific code to create the shortcuts.
class CreateShortcutForCurrentWebContentsTask
    : public content::DocumentUserData<CreateShortcutForCurrentWebContentsTask>,
      public content::WebContentsObserver {
 public:
  // Callback that returns the information in the textfield of the create
  // desktop shortcut dialog if accepted, or std::nullopt otherwise.
  using ShortcutsDialogResultCallback =
      base::OnceCallback<void(std::optional<std::u16string>)>;
  using ShortcutsDialogCallback = base::OnceCallback<void(
      const gfx::ImageSkia& icon_for_dialog,
      std::u16string title_for_dialog,
      ShortcutsDialogResultCallback dialog_result_callback)>;

  // Creates a CreateShortcutForCurrentWebContentsTask for the given
  // `web_contents` and starts it. `dialog_callback` is called when the task
  // needs to show the create shortcut dialog to the user. `callback` is called
  // when the task has completed its execution, including the OS specific calls
  // once the metadata fetching has completed.
  static void CreateAndStart(
      content::WebContents& web_contents,
      ShortcutsDialogCallback dialog_callback,
      base::OnceCallback<void(bool shortcuts_created)> callback);

  ~CreateShortcutForCurrentWebContentsTask() override;

  // content::WebContentsObserver overrides:
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit CreateShortcutForCurrentWebContentsTask(
      content::RenderFrameHost* rfh);

  void FetchIcons(
      content::WebContents& web_contents,
      ShortcutsDialogCallback dialog_callback,
      base::OnceCallback<void(ShortcutCreationTaskResult task_result)>
          callback);
  void OnIconsFetchedStartBadgingAndShowDialog(
      FetchIconsFromDocumentResult result);
  void OnShortcutDialogResultObtained(
      gfx::ImageFamily results,
      GURL shortcut_url,
      std::optional<std::u16string> dialog_result);
  void OnMetadataFetchCompleteSelfDestruct(
      base::expected<ShortcutMetadata, ShortcutCreationTaskResult>
          fetch_result);

  ShortcutsDialogCallback dialog_callback_;
  base::OnceCallback<void(ShortcutCreationTaskResult task_result)> callback_;
  std::unique_ptr<DocumentIconFetcherTask> icon_fetcher_task_;
  base::WeakPtrFactory<CreateShortcutForCurrentWebContentsTask>
      weak_ptr_factory_{this};
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_CREATE_SHORTCUT_FOR_CURRENT_WEB_CONTENTS_TASK_H_
