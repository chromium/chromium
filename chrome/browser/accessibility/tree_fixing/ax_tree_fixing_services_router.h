// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_

#include <list>
#include <memory>

#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screen_ai_service.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ui {
struct AXTreeUpdate;
}  // namespace ui

namespace tree_fixing {

using MainNodeIdentificationCallback =
    base::OnceCallback<void(std::pair<ui::AXTreeID, ui::AXNodeID>)>;

// This class handles the communication between the browser process and any
// downstream services used to fix the AXTree, such as: the optimization guide,
// Screen2x, Aratea, etc.
class AXTreeFixingServicesRouter
    : public KeyedService,
      public AXTreeFixingScreenAIService::MainNodeIdentificationDelegate {
 public:
  explicit AXTreeFixingServicesRouter(Profile* profile);
  AXTreeFixingServicesRouter(const AXTreeFixingServicesRouter&) = delete;
  AXTreeFixingServicesRouter& operator=(const AXTreeFixingServicesRouter&) =
      delete;
  ~AXTreeFixingServicesRouter() override;

  // --- Public APIs for any request to fix an AXTree ---

  // Identifies the main node of an AXTree, and asynchronously returns the
  // identified node_id and its associated tree_id as a std::pair via the
  // provided callback. The AXTreeUpdate that clients provide to this method
  // should represent a full AXTree for the page in order to accurately identify
  // a main node. The AXTree should not have an existing node with Role kMain.
  void IdentifyMainNode(const ui::AXTreeUpdate& ax_tree,
                        MainNodeIdentificationCallback callback);

 private:
  // AXTreeFixingScreenAIService::MainNodeIdentificationDelegate overrides:
  void OnMainNodeIdentified(ui::AXTreeID tree_id,
                            ui::AXNodeID node_id,
                            int request_id) override;

  // ScreenAI related objects: service instance, and a list of callbacks/ids.
  std::unique_ptr<AXTreeFixingScreenAIService> screen_ai_service_;
  std::list<std::pair<int, MainNodeIdentificationCallback>> pending_callbacks_;
  int next_request_id_ = 0;

  // The Profile for the KeyedService for this instance.
  Profile* profile_;
};

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_
