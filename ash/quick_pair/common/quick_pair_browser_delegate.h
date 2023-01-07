// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_QUICK_PAIR_BROWSER_DELEGATE_H_
#define ASH_QUICK_PAIR_COMMON_QUICK_PAIR_BROWSER_DELEGATE_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/services/quick_pair/public/mojom/quick_pair_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class PrefService;

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {
namespace quick_pair {

// Interface for a class which provides browser dependences to classes within
// ash::quick_pair. This allows us to retrieve dependencies (such as the active
// user profile) which cannot directly be retrieved in ash.
class COMPONENT_EXPORT(QUICK_PAIR_COMMON) QuickPairBrowserDelegate {
 public:
  QuickPairBrowserDelegate();
  QuickPairBrowserDelegate(const QuickPairBrowserDelegate&) = delete;
  QuickPairBrowserDelegate& operator=(const QuickPairBrowserDelegate&) = delete;
  virtual ~QuickPairBrowserDelegate();

  static QuickPairBrowserDelegate* Get();

  // Returns the URL loader factory associated with the active user's profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Returns a pointer to the IdentityManager for the active user.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  virtual std::unique_ptr<image_fetcher::ImageFetcher> GetImageFetcher() = 0;

  // For accessing prefs of the active user.
  virtual PrefService* GetActivePrefService() = 0;

  // Starts the utility process which houses QuickPairService and returns a
  // PendingReceiver for it.
  virtual void RequestService(
      mojo::PendingReceiver<mojom::QuickPairService> receiver) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_QUICK_PAIR_BROWSER_DELEGATE_H_
