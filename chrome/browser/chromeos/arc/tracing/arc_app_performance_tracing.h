// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcAppPerformanceTracingSession;
class ArcBridgeService;

// Service that monitors ARC++ apps, measures and reports performance metrics
// for the set of predefined apps.
class ArcAppPerformanceTracing : public KeyedService,
                                 public wm::ActivationChangeObserver,
                                 public aura::WindowObserver,
                                 public ArcAppListPrefs::Observer {
 public:
  using ResultCallback = base::OnceCallback<void(bool success,
                                                 double fps,
                                                 double commit_deviation,
                                                 double render_quality)>;
  using CustomSessionReadyCallback = base::RepeatingCallback<void()>;

  ArcAppPerformanceTracing(content::BrowserContext* context,
                           ArcBridgeService* bridge);
  ~ArcAppPerformanceTracing() override;

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC++.
  static ArcAppPerformanceTracing* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcAppPerformanceTracing* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  static void SetFocusAppForTesting(const std::string& package_name,
                                    const std::string& activity,
                                    const std::string& category);
  void SetCustomSessionReadyCallbackForTesting(
      CustomSessionReadyCallback callback);

  // KeyedService:
  void Shutdown() override;

  // Starts custom tracing. Returns true if tracing was successfully started.
  bool StartCustomTracing();
  // Stops custom tracing and returns tracing results.
  void StopCustomTracing(ResultCallback result_callback);

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // ArcAppListPrefs::Observer:
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent) override;
  void OnTaskDestroyed(int32_t task_id) override;

  // Returns true in case |category| was already reported in the current user's
  // session.
  bool WasReported(const std::string& category) const;
  // Marks that |category| is reported in the current user's session.
  void SetReported(const std::string& category);

  // Returns active tracing session or nullptr.
  ArcAppPerformanceTracingSession* session() { return session_.get(); }

  // Returns currently active ARC window or null. It may or may not be currently
  // profiled.
  aura::Window* active_window() { return arc_active_window_; }

 private:
  // May be start tracing session if all conditions are met. Window creating is
  // controlled by Wayland protocol implementation and task creation is reported
  // using mojom. That introduces the race and following conditions must be met
  // to start the tracing.
  //   * Active window is ARC++ window.
  //   * Task information exists for the window.
  //   * ARC++ app is in set of predefined apps eligible for tracing.
  // This does nothing if session was already started.
  // This is called each time when ARC++ window gets active or ARC++ task
  // creation is reported.
  void MaybeStartTracing();

  // Stops tracing session if it was active and cancels any scheduled session.
  void MaybeStopTracing();

  // Attaches observer to the |window| and stores at as |arc_active_window_|.
  void AttachActiveWindow(aura::Window* window);

  // Detaches observer from |arc_active_window_| and resets
  // |arc_active_window_|.
  void DetachActiveWindow();

  // Unowned pointers.
  content::BrowserContext* const context_;
  // Currently active ARC++ app window.
  aura::Window* arc_active_window_ = nullptr;

  // Maps active tasks to app id.
  std::map<int, std::string> task_id_to_app_id_;

  // Set of already reported ARC++ apps for the current session. Used to prevent
  // capturing too frequently.
  std::set<std::string> reported_categories_;

  // Keeps current active tracing session associated with |arc_active_window_|.
  std::unique_ptr<ArcAppPerformanceTracingSession> session_;

  // Callback to call when custom session is ready for testing.
  CustomSessionReadyCallback custom_session_ready_callback_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppPerformanceTracing);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_H_
