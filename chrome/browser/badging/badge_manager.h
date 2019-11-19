// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"
#include "url/gurl.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace badging {
class BadgeManagerDelegate;

// The maximum value of badge contents before saturation occurs.
constexpr uint64_t kMaxBadgeContent = 99u;

// Maintains a record of badge contents and dispatches badge changes to a
// delegate.
class BadgeManager : public KeyedService, public blink::mojom::BadgeService {
 public:
  // The badge being applied to a URL. If the optional is |base::nullopt| then
  // the badge is "flag". Otherwise the badge is a non-zero integer.
  using BadgeValue = base::Optional<uint64_t>;

  explicit BadgeManager(Profile* profile);
  ~BadgeManager() override;

  // Sets the delegate used for setting/clearing badges.
  void SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate);

  static void BindReceiver(
      content::RenderFrameHost* frame,
      mojo::PendingReceiver<blink::mojom::BadgeService> receiver);

  // Gets the badge for |app_id|. This will be base::nullopt if the app is not
  // badged.
  base::Optional<BadgeValue> GetBadgeValue(const web_app::AppId& app_id);

  void SetBadgeForTesting(const web_app::AppId& app_id, BadgeValue value);
  void ClearBadgeForTesting(const web_app::AppId& app_id);

 private:
  // The BindingContext of a mojo request. Allows mojo calls to be tied back to
  // the |RenderFrameHost| they belong to without trusting the renderer for that
  // information.
  struct BindingContext {
    BindingContext(int process_id, int frame_id)
        : process_id(process_id), frame_id(frame_id) {}
    int process_id;
    int frame_id;
  };

  // Updates the badge for |app_id| to be |value|, if it is not base::nullopt.
  // If value is |base::nullopt| then this clears the badge.
  void UpdateBadge(const web_app::AppId& app_id,
                   base::Optional<BadgeValue> value);

  // blink::mojom::BadgeService:
  // Note: These are private to stop them being called outside of mojo as they
  // require a mojo binding context.
  void SetBadge(blink::mojom::BadgeValuePtr value) override;
  void ClearBadge() override;

  // Gets the app id to badge, based on the |context|. base::nullopt if
  // |context| isn't inside an app.
  base::Optional<web_app::AppId> GetAppIdForBadging(
      const BindingContext& context);

  // All the mojo receivers for the BadgeManager. Keeps track of the
  // render_frame the binding is associated with, so as to not have to rely
  // on the renderer passing it in.
  mojo::ReceiverSet<blink::mojom::BadgeService, BindingContext> receivers_;

  // Delegate which handles actual setting and clearing of the badge.
  // Note: This is currently only set on Windows and MacOS.
  std::unique_ptr<BadgeManagerDelegate> delegate_;

  // Maps app_id to badge contents.
  std::map<web_app::AppId, BadgeValue> badged_apps_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManager);
};

// Determines the text to put on the badge based on some badge_content.
std::string GetBadgeString(BadgeManager::BadgeValue badge_content);

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
