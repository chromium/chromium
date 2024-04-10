// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_MOST_RELEVANT_TAB_RESUMPTION_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_MOST_RELEVANT_TAB_RESUMPTION_PAGE_HANDLER_H_

#include "components/history/core/browser/history_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// The handler for communication between the WebUI.
class MostRelevantTabResumptionPageHandler {
 public:
  explicit MostRelevantTabResumptionPageHandler(
      content::WebContents* web_contents);

  MostRelevantTabResumptionPageHandler(
      const MostRelevantTabResumptionPageHandler&) = delete;
  MostRelevantTabResumptionPageHandler& operator=(
      const MostRelevantTabResumptionPageHandler&) = delete;

  ~MostRelevantTabResumptionPageHandler();

  void GetTabs();

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  base::WeakPtrFactory<MostRelevantTabResumptionPageHandler> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_MOST_RELEVANT_TAB_RESUMPTION_PAGE_HANDLER_H_
