// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_
#define CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "ui/gfx/image/image_skia.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace sharing_hub {

struct SharingHubAction {
  // `icon` may not be null and must outlive the SharingHubAction.
  SharingHubAction(int command_id,
                   std::u16string title,
                   const gfx::VectorIcon* icon,
                   std::string feature_name_for_metrics,
                   int announcement_id);
  SharingHubAction(const SharingHubAction&);
  SharingHubAction& operator=(const SharingHubAction&);
  SharingHubAction(SharingHubAction&&);
  SharingHubAction& operator=(SharingHubAction&&);
  ~SharingHubAction() = default;
  int command_id;
  std::u16string title;
  raw_ref<const gfx::VectorIcon> icon;
  std::string feature_name_for_metrics;
  int announcement_id;
};

// The Sharing Hub model contains a list of first and third party actions.
// This object should only be accessed from one thread, which is usually the
// main thread.
class SharingHubModel {
 public:
  explicit SharingHubModel(content::BrowserContext* context);
  SharingHubModel(const SharingHubModel&) = delete;
  SharingHubModel& operator=(const SharingHubModel&) = delete;
  ~SharingHubModel();

  // Returns a vector with the first party Sharing Hub actions, ordered by
  // appearance in the dialog. Some actions (i.e. send tab to self) may not be
  // shown for some URLs.
  std::vector<SharingHubAction> GetFirstPartyActionList(
      content::WebContents* web_contents) const;

  // Executes the third party action indicated by |id|, i.e. opens a popup to
  // the corresponding webpage. The |url| is the URL to share, and the |title|
  // is the title (if there is one) of the shared URL.
  void ExecuteThirdPartyAction(Profile* profile,
                               const GURL& url,
                               const std::u16string& title,
                               int id);

  // Convenience wrapper around the above when sharing a WebContents. This
  // extracts the title and URL to share from the provided WebContents.
  void ExecuteThirdPartyAction(content::WebContents* contents, int id);

 private:
  void PopulateFirstPartyActions();
  void PopulateThirdPartyActions();

  // A list of Sharing Hub first party actions in order in which they appear.
  std::vector<SharingHubAction> first_party_action_list_;

  raw_ptr<content::BrowserContext> context_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_MODEL_H_
