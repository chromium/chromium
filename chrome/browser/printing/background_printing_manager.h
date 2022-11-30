// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#define CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/sequence_checker.h"

namespace content {
class WebContents;
class BrowserContext;
}

namespace printing {

// Manages hidden WebContents that prints documents in the background.
// The hidden WebContents are no longer part of any Browser / TabStripModel.
// The WebContents started life as a ConstrainedWebDialog.
// They get deleted when the printing finishes.
class BackgroundPrintingManager {
 public:
  class Observer;

  BackgroundPrintingManager();

  BackgroundPrintingManager(const BackgroundPrintingManager&) = delete;
  BackgroundPrintingManager& operator=(const BackgroundPrintingManager&) =
      delete;

  ~BackgroundPrintingManager();

  // Takes ownership of `preview_dialog` and deletes it when `preview_dialog`
  // finishes printing. This removes `preview_dialog` from its ConstrainedDialog
  // and hides it from the user.
  void OwnPrintPreviewDialog(
      std::unique_ptr<content::WebContents> preview_dialog);

  // Returns true if `printing_contents_map_` contains `preview_dialog`.
  bool HasPrintPreviewDialog(content::WebContents* preview_dialog);

  // Let others see the list of background printing contents.
  std::set<content::WebContents*> CurrentContentSet();

  // Delete all preview contents that are associated with `browser_context`.
  void DeletePreviewContentsForBrowserContext(
      content::BrowserContext* browser_context);

  void OnPrintRequestCancelled(content::WebContents* preview_dialog);

 private:
  // Schedule deletion of `preview_contents`.
  void DeletePreviewContents(content::WebContents* preview_contents);

  // A map from print preview WebContentses (managed by
  // BackgroundPrintingManager) to the Observers that observe them and the owned
  // version of the WebContents.
  struct PrintingContents {
    PrintingContents();

    PrintingContents(const PrintingContents&) = delete;
    PrintingContents& operator=(const PrintingContents&) = delete;

    PrintingContents(PrintingContents&&);
    PrintingContents& operator=(PrintingContents&&);

    ~PrintingContents();

    std::unique_ptr<content::WebContents> contents;
    std::unique_ptr<Observer> observer;
  };
  std::map<content::WebContents*, PrintingContents> printing_contents_map_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
