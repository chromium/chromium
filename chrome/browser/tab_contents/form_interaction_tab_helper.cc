// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"

#include "base/functional/bind.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

#if DCHECK_IS_ON()
bool g_observer_exists = false;
#endif

}  // namespace

// Graph observer used to receive the page form interaction events.
class FormInteractionTabHelper::GraphObserver
    : public performance_manager::PageNode::ObserverDefaultImpl,
      public performance_manager::GraphOwned {
 public:
  GraphObserver() = default;
  ~GraphObserver() override = default;
  GraphObserver(const GraphObserver& other) = delete;
  GraphObserver& operator=(const GraphObserver&) = delete;

 private:
  // Should be called on the UI thread to dispatch the OnHadFormInteraction
  // signal received on the PM sequence.
  static void DispatchOnHadFormInteraction(
      base::WeakPtr<content::WebContents> contents,
      bool had_form_interaction);

  // performance_manager::PageNode::ObserverDefaultImpl:
  void OnHadFormInteractionChanged(
      const performance_manager::PageNode* page_node) override;

  // performance_manager::GraphOwned:
  void OnPassedToGraph(performance_manager::Graph* graph) override;
  void OnTakenFromGraph(performance_manager::Graph* graph) override;
};

// static
void FormInteractionTabHelper::GraphObserver::DispatchOnHadFormInteraction(
    base::WeakPtr<content::WebContents> contents,
    bool had_form_interaction) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If the web contents is still alive then dispatch to the actual
  // implementation in TabLifecycleUnitSource.
  if (contents) {
    // Notifications can be emitted by extensions, ignore these.
    if (auto* tab_helper =
            FormInteractionTabHelper::FromWebContents(contents.get())) {
      // Sanity check against spurious changes.
      DCHECK_NE(tab_helper->had_form_interaction_, had_form_interaction);
      tab_helper->had_form_interaction_ = had_form_interaction;
    }
  }
}

void FormInteractionTabHelper::GraphObserver::OnHadFormInteractionChanged(
    const performance_manager::PageNode* page_node) {
  // Forward the notification over to the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GraphObserver::DispatchOnHadFormInteraction,
                                page_node->GetWebContents(),
                                page_node->HadFormInteraction()));
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
