// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/child_process_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"
#include "url/gurl.h"

class Profile;

namespace base {
class Clock;
}  // namespace base

namespace content {
class RenderFrameHost;
class RenderProcessHost;
struct ServiceWorkerVersionBaseInfo;
}  // namespace content

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace badging {
class BadgeManagerDelegate;

// Records different types of update badge event.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum UpdateBadgeType {
  // Set badge to a positive integer value.
  kSetNumericBadge = 0,
  // Set badge without value, display a plain dot.
  kSetFlagBadge = 1,
  // Clear badge with either navigator.setAppBadge(0)
  // or navigator.clearAppBadge().
  kClearBadge = 2,
};

// The maximum value of badge contents before saturation occurs.
inline constexpr uint64_t kMaxBadgeContent = 99u;

// We don't show a badge in response to notifications if the
// Badging API has been used recently.
inline constexpr base::TimeDelta kBadgingOverrideLifetime = base::Days(14);

// We record when the Badging API was last used, but rate limit
// our updates to minimize load on the Web App database,
inline constexpr base::TimeDelta kBadgingMinimumUpdateInterval = base::Hours(2);

// Maintains a record of badge contents and dispatches badge changes to a
// delegate.
class BadgeManager : public KeyedService, public blink::mojom::BadgeService {
 public:
  // The badge being applied to a document URL or service worker scope. If the
  // optional is |std::nullopt| then the badge is "flag". Otherwise the badge
  // is a non-zero integer.
  using BadgeValue = std::optional<uint64_t>;

  explicit BadgeManager(Profile* profile);

  BadgeManager(const BadgeManager&) = delete;
  BadgeManager& operator=(const BadgeManager&) = delete;

  ~BadgeManager() override;

  // Sets the delegate used for setting/clearing badges.
  void SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate);

  static void BindFrameReceiverIfAllowed(
      content::RenderFrameHost* frame,
      mojo::PendingReceiver<blink::mojom::BadgeService> receiver);

  // Binds a remote ServiceWorkerGlobalScope to a badge service.  After
  // receiving a badge update from a ServiceWorkerGlobalScope, the badge
  // service must update the badge for each app under `info.scope`.
  static void BindServiceWorkerReceiverIfAllowed(
      content::RenderProcessHost* service_worker_process_host,
      const content::ServiceWorkerVersionBaseInfo& info,
      mojo::PendingReceiver<blink::mojom::BadgeService> receiver);

  // Gets the badge for |app_id|. This will be std::nullopt if the app is not
  // badged.
  std::optional<BadgeValue> GetBadgeValue(const webapps::AppId& app_id);

  bool HasRecentApiUsage(const webapps::AppId& app_id) const;

  void SetBadgeForTesting(const webapps::AppId& app_id,
                          BadgeValue value,
                          ukm::UkmRecorder* test_recorder);
  void ClearBadgeForTesting(const webapps::AppId& app_id,
                            ukm::UkmRecorder* test_recorder);
  const base::Clock* SetClockForTesting(const base::Clock* clock);
  void FlushReceiversForTesting();

 private:
  // The BindingContext of a mojo request. Allows mojo calls to be tied back
  // to the execution context they belong to without trusting the renderer for
  // that information.  This is an abstract base class that different types of
  // execution contexts derive.
  class BindingContext {
   public:
    virtual ~BindingContext() = default;

    // Gets the list of app IDs to badge, based on the state of this
    // BindingContext.  Returns an empty list when no apps exist for this
    // BindingContext.
    virtual std::vector<std::tuple<webapps::AppId, GURL>>
    GetAppIdsAndUrlsForBadging() const = 0;
  };

  // The BindingContext for Window execution contexts.
  class FrameBindingContext final : public BindingContext {
   public:
    explicit FrameBindingContext(content::GlobalRenderFrameHostId frame_id)
        : frame_id_(frame_id) {}
    ~FrameBindingContext() override = default;

    // Returns the AppId that matches the frame's URL.  Returns either 0 or 1
    // AppIds.
    std::vector<std::tuple<webapps::AppId, GURL>> GetAppIdsAndUrlsForBadging()
        const override;

   private:
    const content::GlobalRenderFrameHostId frame_id_;
  };

  // The BindingContext for ServiceWorkerGlobalScope execution contexts.
  class ServiceWorkerBindingContext final : public BindingContext {
   public:
    ServiceWorkerBindingContext(content::ChildProcessId process_id,
                                const GURL& scope,
                                const blink::StorageKey& storage_key)
        : process_id_(process_id), scope_(scope), storage_key_(storage_key) {}
    ~ServiceWorkerBindingContext() override = default;

    // Returns the list of AppIds within the service worker's scope. Returns
    // either 0, 1 or more AppIds.
    std::vector<std::tuple<webapps::AppId, GURL>> GetAppIdsAndUrlsForBadging()
        const override;

   private:
    const content::ChildProcessId process_id_;
    const GURL scope_;
    const blink::StorageKey storage_key_;
  };

  // Updates the badge for |app_id| to be |value|, if it is not std::nullopt.
  // If value is |std::nullopt| then this clears the badge.
  void UpdateBadge(const webapps::AppId& app_id,
                   std::optional<BadgeValue> value);

  // blink::mojom::BadgeService:
  // Note: These are private to stop them being called outside of mojo as they
  // require a mojo binding context.
  void SetBadge(blink::mojom::BadgeValuePtr value) override;
  void ClearBadge() override;

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  raw_ptr<const base::Clock> clock_;

  // All the mojo receivers for the BadgeManager. Keeps track of each binding's
  // frame or service worker context without relying on renderer-supplied
  // identity.
  mojo::ReceiverSet<blink::mojom::BadgeService, std::unique_ptr<BindingContext>>
      receivers_;

  // Delegate which handles actual setting and clearing of the badge.
  // Note: This is currently set on Windows, MacOS and Chrome OS.
  std::unique_ptr<BadgeManagerDelegate> delegate_;

  // Maps app_id to badge contents.
  std::map<webapps::AppId, BadgeValue> badged_apps_;
};

// Determines the text to put on the badge based on some badge_content.
std::string GetBadgeString(BadgeManager::BadgeValue badge_content);

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
