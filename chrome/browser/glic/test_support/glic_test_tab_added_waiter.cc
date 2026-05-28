// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_tab_added_waiter.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

GlicTestTabAddedWaiter::GlicTestTabAddedWaiter(Profile* profile) {
  tab_observer_ = GlicTabObserver::Create(
      profile, base::BindRepeating(&GlicTestTabAddedWaiter::OnTabEvent,
                                   base::Unretained(this)));
}

GlicTestTabAddedWaiter::~GlicTestTabAddedWaiter() = default;

tabs::TabInterface* GlicTestTabAddedWaiter::Wait() {
  run_loop_.Run();
  return new_tab_;
}

void GlicTestTabAddedWaiter::OnTabEvent(const GlicTabEvent& event) {
  if (auto* creation = std::get_if<TabCreationEvent>(&event)) {
    new_tab_ = creation->new_tab;
    run_loop_.Quit();
  }
}

}  // namespace glic
