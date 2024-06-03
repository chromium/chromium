// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_EVENTS_PROXY_IMPL_H_
#define CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_EVENTS_PROXY_IMPL_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// The implementation of mahi media app events proxy. It forwards MediaApp
// events that the Mahi feature cares about to its observers.
class MahiMediaAppEventsProxyImpl : public chromeos::MahiMediaAppEventsProxy {
 public:
  MahiMediaAppEventsProxyImpl();
  MahiMediaAppEventsProxyImpl(const MahiMediaAppEventsProxyImpl&) = delete;
  MahiMediaAppEventsProxyImpl& operator=(const MahiMediaAppEventsProxyImpl&) =
      delete;
  ~MahiMediaAppEventsProxyImpl() override;

  // chromeos::MahiMediaAppEventsProxy:
  void OnPdfGetFocus(const base::UnguessableToken client_id) override;
  void OnPdfContextMenuShown(const base::UnguessableToken client_id,
                             const gfx::Rect& anchor) override;
  void OnPdfContextMenuHide() override;
  void OnPdfClosed(const base::UnguessableToken client_id) override;
  void AddObserver(Observer*) override;
  void RemoveObserver(Observer*) override;

 private:
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<MahiMediaAppEventsProxyImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_EVENTS_PROXY_IMPL_H_
