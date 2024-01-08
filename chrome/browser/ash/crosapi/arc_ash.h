// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ARC_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_ARC_ASH_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace arc {
class ArcIntentHelperBridge;
}  // namespace arc

namespace crosapi {

// This class is the ash-chrome implementation of Arc interface. This claas must
// only be used from the main thread.
// ArcAsh must be destroyed after ArcIntentHelperBridge destruction.
class ArcAsh : public mojom::Arc,
               public arc::ArcIntentHelperObserver,
               public ProfileObserver {
 public:
  ArcAsh();
  ArcAsh(const ArcAsh&) = delete;
  ArcAsh& operator=(const ArcAsh&) = delete;
  ~ArcAsh() override;

  // If profile_ is already set, ignore the call.
  void MaybeSetProfile(Profile* profile);
  void BindReceiver(mojo::PendingReceiver<mojom::Arc> receiver);

  // crosapi::mojom::Arc:
  void AddObserver(mojo::PendingRemote<mojom::ArcObserver> observer) override;
  void RequestActivityIcons(std::vector<mojom::ActivityNamePtr> activities,
                            mojom::ScaleFactor scale_factor,
                            RequestActivityIconsCallback callback) override;
  void RequestUrlHandlerList(const std::string& url,
                             RequestUrlHandlerListCallback callback) override;
  void RequestTextSelectionActions(
      const std::string& text,
      mojom::ScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) override;
  void HandleUrl(const std::string& url,
                 const std::string& package_name) override;
  void HandleIntent(mojom::IntentInfoPtr intent,
                    mojom::ActivityNamePtr activity) override;
  void AddPreferredPackage(const std::string& package_name) override;
  void IsInstallable(const std::string& package_name,
                     IsInstallableCallback callback) override;

  // arc::ArcIntentHelperObserver:
  void OnIconInvalidated(const std::string& package_name) override;
  void OnArcIntentHelperBridgeShutdown(
      arc::ArcIntentHelperBridge* bridge) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  // Called when activity icons are sent.
  void ConvertActivityIcons(RequestActivityIconsCallback callback,
                            std::vector<arc::mojom::ActivityIconPtr> icons);
  // Called when intent handler list is sent.
  void ConvertIntentHandlerInfo(
      RequestUrlHandlerListCallback callback,
      std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);
  // Called when actions for text selection are sent.
  void ConvertTextSelectionActions(
      RequestTextSelectionActionsCallback callback,
      std::vector<arc::mojom::TextSelectionActionPtr> actions);
  // Called when icon converted to ImageSkia is returned.
  void ConvertTextSelectionAction(
      mojom::TextSelectionActionPtr* converted_action,
      arc::mojom::TextSelectionActionPtr action,
      base::OnceClosure callback,
      const gfx::ImageSkia& image);

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::Arc> receivers_;

  // This class supports any number of observers.
  mojo::RemoteSet<mojom::ArcObserver> observers_;

  // profile_ should not be overridden.
  raw_ptr<Profile> profile_ = nullptr;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ArcAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ARC_ASH_H_
