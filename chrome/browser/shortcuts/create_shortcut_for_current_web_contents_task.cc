// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/create_shortcut_for_current_web_contents_task.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/platform_util.h"  // nogncheck (crbug.com/335727004)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shortcuts/document_icon_fetcher_task.h"
#include "chrome/browser/shortcuts/icon_badging.h"
#include "chrome/browser/shortcuts/shortcut_creator.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"

namespace shortcuts {

namespace {
constexpr int kBestIconSizeForDialog = 32;
}  // namespace

// static
void CreateShortcutForCurrentWebContentsTask::CreateAndStart(
    content::WebContents& web_contents,
    ShortcutsDialogCallback dialog_callback,
    base::OnceCallback<void(bool shortcuts_created)> callback) {
  content::RenderFrameHost* rfh = web_contents.GetPrimaryMainFrame();

  base::OnceCallback<void(ShortcutCreationTaskResult)> task_callback =
      base::BindOnce([](ShortcutCreationTaskResult task_result) {
        base::UmaHistogramEnumeration("Shortcuts.CreationTask.Result",
                                      task_result);
        return task_result ==
               ShortcutCreationTaskResult::kShortcutCreationSuccess;
      }).Then(std::move(callback));

  if (!rfh) {
    std::move(task_callback)
        .Run(ShortcutCreationTaskResult::kInvalidRenderFrameHost);
    return;
  }

  if (GetForCurrentDocument(rfh)) {
    std::move(task_callback)
        .Run(ShortcutCreationTaskResult::kTaskAlreadyRunning);
    return;
  }

  GetOrCreateForCurrentDocument(rfh)->FetchIcons(
      web_contents, std::move(dialog_callback), std::move(task_callback));
}

CreateShortcutForCurrentWebContentsTask::
    ~CreateShortcutForCurrentWebContentsTask() {
  // Although this function calls `OnMetadataFetchCompleteSelfDestruct()`, for
  // this use-case, self-destruction will not occur since this is being
  // triggered as part of destruction itself.
  if (callback_) {
    OnMetadataFetchCompleteSelfDestruct(
        base::unexpected(ShortcutCreationTaskResult::kInvalidRenderFrameHost));
  }
}

void CreateShortcutForCurrentWebContentsTask::PrimaryPageChanged(
    content::Page& page) {
  if (callback_) {
    OnMetadataFetchCompleteSelfDestruct(
        base::unexpected(ShortcutCreationTaskResult::kPageInvalidated));
    return;
  }
}

void CreateShortcutForCurrentWebContentsTask::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN && callback_) {
    OnMetadataFetchCompleteSelfDestruct(
        base::unexpected(ShortcutCreationTaskResult::kPageInvalidated));
    return;
  }
}

DOCUMENT_USER_DATA_KEY_IMPL(CreateShortcutForCurrentWebContentsTask);

CreateShortcutForCurrentWebContentsTask::
    CreateShortcutForCurrentWebContentsTask(content::RenderFrameHost* rfh)
    : content::DocumentUserData<CreateShortcutForCurrentWebContentsTask>(rfh) {}

void CreateShortcutForCurrentWebContentsTask::FetchIcons(
    content::WebContents& web_contents,
    ShortcutsDialogCallback dialog_callback,
    base::OnceCallback<void(ShortcutCreationTaskResult task_result)> callback) {
  dialog_callback_ = std::move(dialog_callback);
  callback_ = std::move(callback);
  Observe(&web_contents);

  icon_fetcher_task_ = std::make_unique<DocumentIconFetcherTask>(
      web_contents, base::BindOnce(&CreateShortcutForCurrentWebContentsTask::
                                       OnIconsFetchedStartBadgingAndShowDialog,
                                   weak_ptr_factory_.GetWeakPtr()));
  icon_fetcher_task_->StartIconFetching();
}

void CreateShortcutForCurrentWebContentsTask::
    OnIconsFetchedStartBadgingAndShowDialog(
        FetchIconsFromDocumentResult result) {
  CHECK(icon_fetcher_task_);
  icon_fetcher_task_.reset();
  if (!result.has_value()) {
    OnMetadataFetchCompleteSelfDestruct(
        base::unexpected(ShortcutCreationTaskResult::kIconFetchingFailed));
    return;
  }

  if (!web_contents()) {
    OnMetadataFetchCompleteSelfDestruct(
        base::unexpected(ShortcutCreationTaskResult::kInvalidRenderFrameHost));
    return;
  }

  gfx::ImageFamily badged_images = ApplyProductLogoBadgeToIcons(result.value());
  CHECK(!badged_images.empty());

  // GetBest() is guaranteed to not return null since badged_images is not
  // empty.
  const gfx::Image* best_image_for_dialog = badged_images.GetBest(
      gfx::Size(kBestIconSizeForDialog, kBestIconSizeForDialog));
  CHECK(best_image_for_dialog);

  auto results_callback = base::BindOnce(
      &CreateShortcutForCurrentWebContentsTask::OnShortcutDialogResultObtained,
      weak_ptr_factory_.GetWeakPtr(), std::move(badged_images),
      web_contents()->GetLastCommittedURL());
  std::move(dialog_callback_)
      .Run(best_image_for_dialog->AsImageSkia(), web_contents()->GetTitle(),
           std::move(results_callback));
}

void CreateShortcutForCurrentWebContentsTask::OnShortcutDialogResultObtained(
    gfx::ImageFamily images,
    GURL shortcut_url,
    std::optional<std::u16string> dialog_result) {
  if (!dialog_result.has_value()) {
    OnMetadataFetchCompleteSelfDestruct(base::unexpected(
        ShortcutCreationTaskResult::kUserCancelledShortcutCreationFromDialog));
    return;
  }

  // The title returned from the dialog is expected to be non-empty if
  // dialog_result is true, which is an invariant of how the create shortcut
  // view works.
  std::u16string title = dialog_result.value();
  CHECK(!title.empty());

  ShortcutMetadata metadata;
  metadata.shortcut_title = title;
  metadata.shortcut_images = std::move(images);
  metadata.shortcut_url = shortcut_url;

  CHECK(metadata.shortcut_url.is_valid());

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  metadata.profile_path = profile->GetPath();

  CHECK(!metadata.profile_path.empty());

  OnMetadataFetchCompleteSelfDestruct(std::move(metadata));
}

void CreateShortcutForCurrentWebContentsTask::
    OnMetadataFetchCompleteSelfDestruct(
        base::expected<ShortcutMetadata, ShortcutCreationTaskResult>
            fetch_result) {
  if (!fetch_result.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  /*task_result=*/fetch_result.error()));
  } else {
    auto result_callback =
        base::BindOnce([](const base::FilePath& shortcut_path,
                          ShortcutCreatorResult result) {
          base::UmaHistogramEnumeration("Shortcuts.Creation.Result", result);
          if (result != ShortcutCreatorResult::kError &&
              base::FeatureList::IsEnabled(
                  features::kShortcutsNotAppsRevealDesktop)) {
            CHECK(!shortcut_path.empty());
            // Profile information is not needed to show the created shortcut in
            // the path on Windows, Mac and Linux.
            platform_util::ShowItemInFolder(/*profile=*/nullptr, shortcut_path);
          }
          return (result == ShortcutCreatorResult::kError)
                     ? ShortcutCreationTaskResult::kShortcutCreationFailure
                     : ShortcutCreationTaskResult::kShortcutCreationSuccess;
        }).Then(base::BindOnce(std::move(callback_)));

    GetShortcutsTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&shortcuts::CreateShortcutOnUserDesktop,
                                  std::move(fetch_result.value()),
                                  base::BindPostTaskToCurrentDefault(
                                      std::move(result_callback))));
  }

  // PrimaryPageChanged() already deletes the current page metadata, in which
  // case, no need of calling DeleteForCurrentDocument(). In fact, doing so
  // causes this to crash.
  if (GetForCurrentDocument(&render_frame_host())) {
    DeleteForCurrentDocument(&render_frame_host());
  }
}

}  // namespace shortcuts
