// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_
#define CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_

#include <vector>

#include "base/macros.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace sharing_hub {

struct SharingHubAction {
  int command_id;
  int title;
  const gfx::VectorIcon& icon;
  bool is_first_party;
};

// The Sharing Hub model contains a list of first and third party actions.
// This object should only be accessed from one thread, which is usually the
// main thread.
class SharingHubModel {
 public:
  explicit SharingHubModel(content::BrowserContext* context);
  ~SharingHubModel();

  // Populates the vector with Sharing Hub actions, ordered by appearance in the
  // dialog. Some actions (i.e. send tab to self) may not be shown for some
  // URLs.
  void GetActionList(content::WebContents* web_contents,
                     std::vector<SharingHubAction>* list);

 private:
  void PopulateFirstPartyActions();
  void PopulateThirdPartyActions();

  bool DoShowSendTabToSelfForWebContents(content::WebContents* web_contents);

  // A list of Sharing Hub actions in order in which they appear.
  std::vector<SharingHubAction> action_list_;

  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(SharingHubModel);
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_
