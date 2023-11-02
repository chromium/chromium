// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NATIVE_THEME_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_NATIVE_THEME_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/native_theme.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ui {
class NativeTheme;
}

namespace crosapi {

// The ash-chrome implementation of the NativeTheme crosapi interface.
class NativeThemeServiceAsh : public mojom::NativeThemeService {
 public:
  // Helper to observe changes themes, compile these values, and
  // manage / dispatch to observers of NativeThemeServiceAsh.
  class Dispatcher : public ui::NativeThemeObserver {
   public:
    Dispatcher();
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;
    ~Dispatcher() override;

    // ui::NativeThemeObserver:
    void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

   private:
    friend NativeThemeServiceAsh;

    // Support any number of observers.
    mojo::RemoteSet<mojom::NativeThemeInfoObserver> observers_;
  };

  // Returns a new mojom::NativeThemeInfoPtr populated with latest Ash data.
  static mojom::NativeThemeInfoPtr GetNativeThemeInfo();

  NativeThemeServiceAsh();
  NativeThemeServiceAsh(const NativeThemeServiceAsh&) = delete;
  NativeThemeServiceAsh& operator=(const NativeThemeServiceAsh&) = delete;
  ~NativeThemeServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::NativeThemeService> receiver);

  // crosapi::mojom::NativeThemeService:
  void AddNativeThemeInfoObserver(
      mojo::PendingRemote<mojom::NativeThemeInfoObserver> observer) override;

 private:
  // Support any number of connections.
  mojo::ReceiverSet<mojom::NativeThemeService> receivers_;

  Dispatcher dispatcher_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NATIVE_THEME_SERVICE_ASH_H_
