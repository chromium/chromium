// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_OBSERVER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_OBSERVER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/infobars/core/infobar_manager.h"

// A test-only class to wait for infobar events.
class InfoBarObserver : public infobars::InfoBarManager::Observer {
 public:
  enum class Type {
    kInfoBarAdded,
    kInfoBarRemoved,
    kInfoBarReplaced,
  };

  // Creates the observer. |type| is the type of infobar event that should be
  // waited for.
  InfoBarObserver(infobars::InfoBarManager* manager, Type type);

  InfoBarObserver(const InfoBarObserver&) = delete;
  InfoBarObserver& operator=(const InfoBarObserver&) = delete;

  ~InfoBarObserver() override;

  // Waits for the specified infobar event to happen. It is OK if the infobar
  // event happens before Wait() is called, as long as the event happens after
  // this object is constructed.
  void Wait();

 private:
  // infobars::InfoBarManager::Observer:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

  void OnNotified(Type type);

  base::RunLoop run_loop_;
  const Type type_;
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_observation_{this};
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_OBSERVER_H_
