// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_picker_interceptor.h"

#include <utility>

#include "base/containers/extend.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/escape.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {

namespace {

using ::blink::mojom::FileChooserFileInfo;
using ::blink::mojom::FileChooserFileInfoPtr;
using ::blink::mojom::FileChooserParams;

// Builds the `filesystem:` URL that names `file` for `origin`. The isolated
// file system id is origin independent, so the URL can be built lazily, once
// the frame requesting the chooser is known.
//
// The registered name is a file name, not a URL fragment, so it is escaped:
// otherwise characters such as '#', '?' and '%' would end the path or be
// decoded as escapes. Both the file system backend and Blink (for File.name)
// unescape the last path component, so the original name is preserved.
GURL BuildFileSystemURL(const url::Origin& origin,
                        const FilePickerInterceptor::StagedFile& file) {
  return GURL(storage::GetIsolatedFileSystemRootURIString(
                  origin.GetURL(), file.filesystem_id(), std::string())
                  .append(base::EscapePath(file.register_name())));
}

// Whether `url` cracks back to `file`'s isolated file system.
//
// FileChooserImpl grants the renderer read access to whatever file system the
// URL cracks to, and does not handle URLs that fail to crack: it passes the
// resulting empty id on to IsolatedContext, which crashes. Only URLs that are
// known to name `file` may therefore be handed out.
bool ResolvesToStagedFile(storage::FileSystemContext& context,
                          const GURL& url,
                          const FilePickerInterceptor::StagedFile& file) {
  const storage::FileSystemURL cracked =
      context.CrackURLInFirstPartyContext(url);
  return cracked.is_valid() &&
         cracked.type() == storage::kFileSystemTypeInMemory &&
         cracked.mount_filesystem_id() == file.filesystem_id();
}

// Keeps the files handed to a document readable for as long as the document
// exists. Files from several uploads to the same document, e.g. for different
// inputs of one form, are all kept.
class HandedOutFiles : public content::DocumentUserData<HandedOutFiles> {
 public:
  void Add(std::vector<FilePickerInterceptor::StagedFile> files) {
    base::Extend(files_, std::move(files));
  }

 private:
  explicit HandedOutFiles(content::RenderFrameHost* render_frame_host)
      : DocumentUserData(render_frame_host) {}

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  std::vector<FilePickerInterceptor::StagedFile> files_;
};

DOCUMENT_USER_DATA_KEY_IMPL(HandedOutFiles);

}  // namespace

FilePickerInterceptor::FilePickerInterceptor(
    content::WebContents* web_contents,
    content::WeakDocumentPtr target_document,
    std::vector<StagedFile> files,
    base::TimeDelta timeout)
    : content::WebContentsUserData<FilePickerInterceptor>(*web_contents),
      content::WebContentsObserver(web_contents),
      target_document_(std::move(target_document)),
      files_(std::move(files)) {
  staging_timer_.Start(FROM_HERE, timeout,
                       base::BindOnce(&FilePickerInterceptor::OnStagingTimeout,
                                      base::Unretained(this)));
}

FilePickerInterceptor::~FilePickerInterceptor() = default;

// static
void FilePickerInterceptor::Install(content::RenderFrameHost& target_frame,
                                    std::vector<StagedFile> files,
                                    base::TimeDelta timeout) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&target_frame);
  // Only one staging can be active at a time; drop any previous one.
  Remove(*web_contents);
  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new FilePickerInterceptor(
                                web_contents, target_frame.GetWeakDocumentPtr(),
                                std::move(files), timeout)));
}

// static
void FilePickerInterceptor::Remove(content::WebContents& web_contents) {
  web_contents.RemoveUserData(UserDataKey());
}

bool FilePickerInterceptor::HandleFileChooser(
    content::RenderFrameHost& render_frame_host,
    scoped_refptr<content::FileSelectListener>& listener,
    const FileChooserParams& params) {
  // The files were staged for one document only. Any other document in the
  // tab, e.g. a cross-origin subframe, must not receive them.
  if (target_document_.AsRenderFrameHostIfValid() != &render_frame_host) {
    return false;
  }

  if (files_.empty()) {
    return false;
  }

  // Only plain file open requests are answered. Save dialogs and directory
  // uploads are left to the regular path.
  if (params.mode != FileChooserParams::Mode::kOpen &&
      params.mode != FileChooserParams::Mode::kOpenMultiple) {
    return false;
  }

  // Staged files exist only in memory and therefore have no native path. A
  // caller that needs one (e.g. `<input type=file webkitdirectory>`) has to go
  // through the regular file picker.
  if (params.need_local_path) {
    return false;
  }

  const url::Origin origin = render_frame_host.GetLastCommittedOrigin();
  storage::FileSystemContext* file_system_context =
      render_frame_host.GetStoragePartition()->GetFileSystemContext();
  std::vector<FileChooserFileInfoPtr> chooser_files;
  std::vector<StagedFile> handed_out_files;
  for (StagedFile& file : files_) {
    GURL url = BuildFileSystemURL(origin, file);
    if (!url.is_valid() ||
        !ResolvesToStagedFile(*file_system_context, url, file)) {
      continue;
    }
    chooser_files.push_back(FileChooserFileInfo::NewFileSystem(
        blink::mojom::FileSystemFileInfo::New(
            std::move(url), file.modification_time(), file.length())));
    handed_out_files.push_back(std::move(file));

    if (params.mode == FileChooserParams::Mode::kOpen) {
      // The input only accepts a single file.
      break;
    }
  }

  if (chooser_files.empty()) {
    return false;
  }

  // The renderer reads the files lazily, e.g. when the form is submitted, so
  // they must stay alive for as long as the document they are handed to.
  HandedOutFiles::GetOrCreateForCurrentDocument(&render_frame_host)
      ->Add(std::move(handed_out_files));
  listener->FileSelected(std::move(chooser_files), base::FilePath(),
                         params.mode);
  listener.reset();

  // The staging is used up. Staged files that were not handed out are
  // discarded; `this` is deleted.
  Remove(GetWebContents());
  return true;
}

void FilePickerInterceptor::OnStagingTimeout() {
  // The page never opened a file chooser. Discard the staged files; `this` is
  // deleted.
  Remove(GetWebContents());
}

void FilePickerInterceptor::PrimaryPageChanged(content::Page& page) {
  // The page was navigated away from before it opened a file chooser. Discard
  // the staged files so that they are not handed to the new page; `this` is
  // deleted.
  Remove(GetWebContents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FilePickerInterceptor);

bool MaybeInterceptFileChooser(
    content::RenderFrameHost& render_frame_host,
    scoped_refptr<content::FileSelectListener>& listener,
    const FileChooserParams& params) {
  if (!base::FeatureList::IsEnabled(kGlicActorFileUploadTool)) {
    return false;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  if (!web_contents) {
    return false;
  }

  FilePickerInterceptor* interceptor =
      FilePickerInterceptor::FromWebContents(web_contents);
  if (!interceptor) {
    return false;
  }

  return interceptor->HandleFileChooser(render_frame_host, listener, params);
}

}  // namespace actor
