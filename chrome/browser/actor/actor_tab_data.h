// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TAB_DATA_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TAB_DATA_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actor {
namespace ui {
class DomNodeGeometry;
}

// Represents a data that the actor service stores for a tab.
class ActorTabData {
 public:
  explicit ActorTabData(tabs::TabInterface* tab);
  ~ActorTabData();

  DECLARE_USER_DATA(ActorTabData);
  static ActorTabData* From(tabs::TabInterface* tab);

  void DidObserveContent(
      optimization_guide::proto::AnnotatedPageContent& content);

  // Returns last observed page content, nullptr if no observation has been
  // made.
  const optimization_guide::proto::AnnotatedPageContent*
  GetLastObservedPageContent();

  const ui::DomNodeGeometry* GetLastObservedDomNodeGeometry();

 private:
  // Stores the last observed page content for TOCTOU check.
  std::optional<optimization_guide::proto::AnnotatedPageContent>
      last_observed_page_content_;
  std::unique_ptr<ui::DomNodeGeometry> last_observed_dom_node_geometry_;

  ::ui::ScopedUnownedUserData<ActorTabData> scoped_unowned_user_data_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TAB_DATA_H_
