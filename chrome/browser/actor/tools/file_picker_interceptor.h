// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FILE_PICKER_INTERCEPTOR_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FILE_PICKER_INTERCEPTOR_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "storage/browser/file_system/in_memory_file_registry.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-forward.h"

namespace content {
class FileSelectListener;
class Page;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace actor {

// Holds the files an actor task has staged for upload to a page, and answers
// the page's file chooser request with them instead of letting the native file
// picker be shown.
//
// The interceptor is installed for the frame holding the target file input by
// FileUploadTool, just before the input is clicked.
// `FileSelectHelper::RunFileChooser()` consults it via
// `MaybeInterceptFileChooser()`; if the request is handled here, no file picker
// dialog is ever created. Only requests from the target frame's document are
// answered, so other frames in the tab, e.g. cross-origin subframes, never get
// the staged files.
//
// The staged files live entirely in browser process memory (see
// storage::InMemoryFileRegistry); nothing is ever written to disk, so there is
// no temporary file to leak if the browser exits unexpectedly.
//
// The staging is discarded if another upload replaces it, the staging timeout
// fires, or the page is navigated away from before a file chooser request
// arrives. Once a request is answered, the interceptor removes itself, and the
// files it handed out are kept for as long as the requesting document exists:
// the renderer reads their contents lazily, e.g. when the form is finally
// submitted, and nothing can read them once the document is gone.
class FilePickerInterceptor
    : public content::WebContentsUserData<FilePickerInterceptor>,
      public content::WebContentsObserver {
 public:
  using StagedFile = storage::InMemoryFileRegistry::ScopedFile;

  FilePickerInterceptor(const FilePickerInterceptor&) = delete;
  FilePickerInterceptor& operator=(const FilePickerInterceptor&) = delete;
  ~FilePickerInterceptor() override;

  // Stages `files` for the current document of `target_frame`, replacing any
  // staging in its tab that has not been used yet. If no file chooser request
  // arrives within `timeout`, or before the page is navigated away from, the
  // staging is discarded.
  static void Install(content::RenderFrameHost& target_frame,
                      std::vector<StagedFile> files,
                      base::TimeDelta timeout);

  // Discards the staging on `web_contents`, if any. Files that were already
  // handed to a page are not affected.
  static void Remove(content::WebContents& web_contents);

  // Fulfills a file chooser request using the staged files, then removes the
  // interceptor, deleting `this`. Returns false if the request cannot be
  // handled, e.g. because it does not come from the target document, in which
  // case the caller must fall back to the normal file picker path, `listener`
  // is left untouched and the staging stays in place.
  bool HandleFileChooser(content::RenderFrameHost& render_frame_host,
                         scoped_refptr<content::FileSelectListener>& listener,
                         const blink::mojom::FileChooserParams& params);

 private:
  friend class content::WebContentsUserData<FilePickerInterceptor>;

  FilePickerInterceptor(content::WebContents* web_contents,
                        content::WeakDocumentPtr target_document,
                        std::vector<StagedFile> files,
                        base::TimeDelta timeout);

  void OnStagingTimeout();

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // The only document whose file chooser requests are answered.
  const content::WeakDocumentPtr target_document_;
  std::vector<StagedFile> files_;
  base::OneShotTimer staging_timer_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Returns true if an actor task has staged files for the document in
// `render_frame_host` and the chooser request was answered with them.
// `listener` is only consumed when this returns true.
bool MaybeInterceptFileChooser(
    content::RenderFrameHost& render_frame_host,
    scoped_refptr<content::FileSelectListener>& listener,
    const blink::mojom::FileChooserParams& params);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FILE_PICKER_INTERCEPTOR_H_
