// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/check.h"
#include "base/functional/callback.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/printing/common/print.mojom.h"

class GURL;

namespace content {
class WebContents;
}

namespace ui {
class WebDialogDelegate;
}

namespace printing {

// For print preview, the WebContents that initiates the printing operation is
// the initiator, and the constrained dialog that shows the print preview is the
// print preview dialog.
// This class manages print preview dialog creation and destruction, and keeps
// track of the 1:1 relationship between initiator tabs and print preview
// dialogs.
class PrintPreviewDialogController : public WebContentsCollection::Observer {
 public:
  // Should only be used by `BrowserProcess`-like classes. Call `GetInstance()`
  // to get the active instance.
  PrintPreviewDialogController();

  PrintPreviewDialogController(const PrintPreviewDialogController&) = delete;
  PrintPreviewDialogController& operator=(const PrintPreviewDialogController&) =
      delete;

  ~PrintPreviewDialogController() override;

  // Returns the existing instance in the global `BrowserProcess`.
  static PrintPreviewDialogController* GetInstance();

  // Returns true if `url` is a Print Preview dialog URL (has `chrome://print`
  // origin).
  static bool IsPrintPreviewURL(const GURL& url);

  // Returns true if `url` is a Print Preview content URL (has
  // `chrome-untrusted://print` origin).
  static bool IsPrintPreviewContentURL(const GURL& url);

  // Initiates print preview for `initiator`.
  void PrintPreview(content::WebContents* initiator,
                    const mojom::RequestPrintPreviewParams& params);

  // Returns the preview dialog for `contents`.
  // Returns `contents` if `contents` is a preview dialog.
  // Returns nullptr if no preview dialog exists for `contents`.
  content::WebContents* GetPrintPreviewForContents(
      content::WebContents* contents) const;

  // Returns the initiator for `preview_dialog`.
  // Returns nullptr if no initiator exists for `preview_dialog`.
  content::WebContents* GetInitiator(content::WebContents* preview_dialog);

  // Returns the request data associated with `preview_dialog`.
  // Returns nullptr if no data exists for `preview_dialog`.
  const mojom::RequestPrintPreviewParams* GetRequestParams(
      content::WebContents* preview_dialog) const;

  // Runs `callback` on the dialog of each active print preview operation.
  void ForEachPreviewDialog(
      base::RepeatingCallback<void(content::WebContents*)> callback);

  // Erases the initiator info associated with `preview_dialog`.
  void EraseInitiatorInfo(content::WebContents* preview_dialog);

  static std::unique_ptr<ui::WebDialogDelegate>
  CreatePrintPreviewDialogDelegateForTesting(content::WebContents* initiator);

  // Exposes GetOrCreatePreviewDialog() for testing.
  content::WebContents* GetOrCreatePreviewDialogForTesting(
      content::WebContents* initiator);

#if defined(UNIT_TEST)
  // Exposes a way for tests to manually specify the initiator to preview_dialog
  // relationship. For use in tests that create their own preview dialogs.
  void AssociateWebContentsesForTesting(content::WebContents* initiator,
                                        content::WebContents* preview_dialog) {
    CHECK(initiator);
    CHECK(preview_dialog);
    mojom::RequestPrintPreviewParams params;
    params.is_modifiable = true;
    InitiatorData data(initiator, params, /*scoper=*/nullptr);
    preview_dialog_map_.emplace(preview_dialog, std::move(data));
  }
  void DisassociateWebContentsesForTesting(
      content::WebContents* preview_dialog) {
    CHECK(preview_dialog);
    size_t erased_count = preview_dialog_map_.erase(preview_dialog);
    CHECK(erased_count);
  }
#endif

  bool is_creating_print_preview_dialog() const {
    return is_creating_print_preview_dialog_;
  }

 private:
  // Tracks the initiator, as well as some of its Print Preview properties.
  struct InitiatorData {
    InitiatorData(content::WebContents* initiator,
                  const mojom::RequestPrintPreviewParams& request_params,
                  std::unique_ptr<tabs::ScopedTabModalUI> scoper);
    InitiatorData(InitiatorData&&) noexcept;
    InitiatorData& operator=(InitiatorData&&) noexcept;
    ~InitiatorData();

    raw_ptr<content::WebContents> initiator;
    mojom::RequestPrintPreviewParams request_params;

    // Prevents other tab-modal UIs from showing.
    std::unique_ptr<tabs::ScopedTabModalUI> scoper;
  };

  // 1:1 relationship between a print preview dialog and its initiator data.
  // Key: Print preview dialog.
  // Value: Initiator data.
  using PrintPreviewDialogMap = std::map<content::WebContents*, InitiatorData>;

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
      content::WebContents* initiator,
      const mojom::RequestPrintPreviewParams& params);

  // Creates a new print preview dialog if GetOrCreatePreviewDialog() cannot
  // find a print preview dialog for `initiator`.
  content::WebContents* CreatePrintPreviewDialog(
      tabs::TabInterface* tab,
      content::WebContents* initiator,
      const mojom::RequestPrintPreviewParams& params);

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
