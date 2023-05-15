// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"

class GURL;

namespace content {
class WebContents;
}

namespace printing {

// For print preview, the WebContents that initiates the printing operation is
// the initiator, and the constrained dialog that shows the print preview is the
// print preview dialog.
// This class manages print preview dialog creation and destruction, and keeps
// track of the 1:1 relationship between initiator tabs and print preview
// dialogs.
class PrintPreviewDialogController
    : public base::RefCounted<PrintPreviewDialogController>,
      public WebContentsCollection::Observer {
 public:
  PrintPreviewDialogController();

  PrintPreviewDialogController(const PrintPreviewDialogController&) = delete;
  PrintPreviewDialogController& operator=(const PrintPreviewDialogController&) =
      delete;

  static PrintPreviewDialogController* GetInstance();

  // Returns true if `url` is a Print Preview dialog URL (has `chrome://print`
  // origin).
  static bool IsPrintPreviewURL(const GURL& url);

  // Returns true if `url` is a Print Preview content URL (has
  // `chrome-untrusted://print` origin).
  static bool IsPrintPreviewContentURL(const GURL& url);

  // Initiates print preview for `initiator`.
  void PrintPreview(content::WebContents* initiator);

  // Returns the preview dialog for `contents`.
  // Returns `contents` if `contents` is a preview dialog.
  // Returns NULL if no preview dialog exists for `contents`.
  content::WebContents* GetPrintPreviewForContents(
      content::WebContents* contents) const;

  // Returns the initiator for `preview_dialog`.
  // Returns NULL if no initiator exists for `preview_dialog`.
  content::WebContents* GetInitiator(content::WebContents* preview_dialog);

  // Runs `callback` on the dialog of each active print preview operation.
  void ForEachPreviewDialog(
      base::RepeatingCallback<void(content::WebContents*)> callback);

  // Erases the initiator info associated with `preview_dialog`.
  void EraseInitiatorInfo(content::WebContents* preview_dialog);

  // Exposes GetOrCreatePreviewDialog() for testing.
  content::WebContents* GetOrCreatePreviewDialogForTesting(
      content::WebContents* initiator);

  bool is_creating_print_preview_dialog() const {
    return is_creating_print_preview_dialog_;
  }

 private:
  friend class base::RefCounted<PrintPreviewDialogController>;

  // 1:1 relationship between a print preview dialog and its initiator tab.
  // Key: Print preview dialog.
  // Value: Initiator.
  using PrintPreviewDialogMap =
      std::map<content::WebContents*, content::WebContents*>;

  ~PrintPreviewDialogController() override;

  // WebContentsCollection::Observer:
  // Handles the closing of the RenderProcessHost. This is observed when the
  // initiator renderer crashes.
  void RenderProcessGone(content::WebContents* contents,
                         base::TerminationStatus status) override;

  // Handles the destruction of `contents`. This is observed when either
  // the initiator or preview WebContents is closed.
  void WebContentsDestroyed(content::WebContents* contents) override;

  // Handles the commit of a navigation for `contents`. This is observed when
  // the renderer for either WebContents is navigated to a different page.
  void DidFinishNavigation(
      content::WebContents* contents,
      content::NavigationHandle* navigation_handle) override;

  // Helpers for DidFinishNavigation().
  void OnInitiatorNavigated(content::WebContents* initiator,
                            content::NavigationHandle* navigation_handle);
  void OnPreviewDialogNavigated(content::WebContents* preview_dialog,
                                content::NavigationHandle* navigation_handle);

  // Gets/Creates the print preview dialog for `initiator`.
  content::WebContents* GetOrCreatePreviewDialog(
      content::WebContents* initiator);

  // Creates a new print preview dialog if GetOrCreatePreviewDialog() cannot
  // find a print preview dialog for `initiator`.
  content::WebContents* CreatePrintPreviewDialog(
      content::WebContents* initiator);

  // Helper function to store the title of the initiator associated with
  // `preview_dialog` in `preview_dialog`'s PrintPreviewUI.
  void SaveInitiatorTitle(content::WebContents* preview_dialog);

  // Removes WebContents when they close/crash/navigate.
  void RemoveInitiator(content::WebContents* initiator);
  void RemovePreviewDialog(content::WebContents* preview_dialog);

  // Mapping between print preview dialog and the corresponding initiator.
  PrintPreviewDialogMap preview_dialog_map_;

  WebContentsCollection web_contents_collection_;

  // Whether the PrintPreviewDialogController is in the middle of creating a
  // print preview dialog.
  bool is_creating_print_preview_dialog_ = false;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_
