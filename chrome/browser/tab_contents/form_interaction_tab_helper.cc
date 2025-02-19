// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"

#include "base/functional/bind.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"

namespace {

#if DCHECK_IS_ON()
bool g_observer_exists = false;
#endif

}  // namespace

// Graph observer used to receive the page form interaction events.
class FormInteractionTabHelper::GraphObserver
    : public performance_manager::PageNodeObserver,
      public performance_manager::GraphOwned {
 public:
  GraphObserver() = default;
  ~GraphObserver() override = default;
  GraphObserver(const GraphObserver& other) = delete;
  GraphObserver& operator=(const GraphObserver&) = delete;

 private:
  // performance_manager::PageNodeObserver:
  void OnHadFormInteractionChanged(
      const performance_manager::PageNode* page_node) override;

  // performance_manager::GraphOwned:
  void OnPassedToGraph(performance_manager::Graph* graph) override;
  void OnTakenFromGraph(performance_manager::Graph* graph) override;
};

void FormInteractionTabHelper::GraphObserver::OnHadFormInteractionChanged(
    const performance_manager::PageNode* page_node) {
  base::WeakPtr<content::WebContents> contents = page_node->GetWebContents();
  CHECK(contents);
  bool had_form_interaction = page_node->HadFormInteraction();

  // Notifications can be emitted by extensions, ignore these.
  if (auto* tab_helper =
          FormInteractionTabHelper::FromWebContents(contents.get())) {
    // Sanity check against spurious changes.
    DCHECK_NE(tab_helper->had_form_interaction_, had_form_interaction);
    tab_helper->had_form_interaction_ = had_form_interaction;
  }
}

void FormInteractionTabHelper::GraphObserver::OnPassedToGraph(
    performance_manager::Graph* graph) {
#if DCHECK_IS_ON()
  DCHECK(!g_observer_exists);
  g_observer_exists = true;
#endif
  graph->AddPageNodeObserver(this);
}

void FormInteractionTabHelper::GraphObserver::OnTakenFromGraph(
    performance_manager::Graph* graph) {
#if DCHECK_IS_ON()
  DCHECK(g_observer_exists);
  g_observer_exists = false;
#endif
  graph->RemovePageNodeObserver(this);
}

// static
std::unique_ptr<performance_manager::GraphOwned>
FormInteractionTabHelper::CreateGraphObserver() {
  return std::make_unique<FormInteractionTabHelper::GraphObserver>();
}

FormInteractionTabHelper::FormInteractionTabHelper(
    content::WebContents* contents)
    : content::WebContentsUserData<FormInteractionTabHelper>(*contents) {}

FormInteractionTabHelper::~FormInteractionTabHelper() = default;

bool FormInteractionTabHelper::had_form_interaction() const {
#if DCHECK_IS_ON()
  // The observer is allowed to not exist in tests that don't use
  // PerformanceManager, in which case this function will always return false.
  DCHECK(g_observer_exists ||
         !performance_manager::PerformanceManager::IsAvailable());
#endif
  return had_form_interaction_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FormInteractionTabHelper);
