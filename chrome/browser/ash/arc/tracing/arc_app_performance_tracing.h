// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "ash/components/arc/mojom/metrics.mojom.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "components/exo/surface_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace exo {
class ScopedSurface;
}  // namespace exo

namespace arc {

class ArcAppPerformanceTracingSession;
class ArcBridgeService;

// Service that monitors ARC++ apps, measures and reports performance metrics
// for the set of predefined apps. Also report GFX metrics jankiness results.
class ArcAppPerformanceTracing : public KeyedService,
                                 public wm::ActivationChangeObserver,
                                 public aura::WindowObserver,
                                 public ArcAppListPrefs::Observer,
                                 public exo::SurfaceObserver {
 public:
  using ResultCallback = base::OnceCallback<void(bool success,
                                                 double fps,
                                                 double commit_deviation,
                                                 double render_quality)>;
  using CustomSessionReadyCallback = base::RepeatingCallback<void()>;

  ArcAppPerformanceTracing(content::BrowserContext* context,
                           ArcBridgeService* bridge);

  ArcAppPerformanceTracing(const ArcAppPerformanceTracing&) = delete;
  ArcAppPerformanceTracing& operator=(const ArcAppPerformanceTracing&) = delete;

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
                     const std::string& intent,
                     int32_t session_id) override;
  void OnTaskDestroyed(int32_t task_id) override;

  // exo::SurfaceObserver:
  void OnCommit(exo::Surface* surface) override;
  void OnSurfaceDestroying(exo::Surface* surface) override;

  void HandleActiveAppRendered(base::Time timestamp);

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
  //   * User has app syncing enabled and no sync passphrase.
  //   * Stats reporting is enabled for user.
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

  // Starts timer for jankiness tracing. Called by OnWindowActivation() and
  // FinalizeJankinessTracing().
  void StartJankinessTracing();

  // Cancels jankiness tracing without reporting partial results.
  void CancelJankinessTracing();

  // Retrieves and reports jankiness metrics and restarts timer. May be called
  // early by OnWindowActivation() and OnWindowDestroying().
  // In this case, |stopped_early| is set to true.
  void FinalizeJankinessTracing(bool stopped_early);

  // Callback for jankiness results. Reports results to UMA.
  // Note: Results are cumulative. Uses task_id_to_gfx_metrics_
  // values for delta calculations.
  void OnGfxMetrics(const std::string& package_name,
                    mojom::GfxMetricsPtr metrics_ptr);

  // Unowned pointers.
  content::BrowserContext* const context_;
  // Currently active ARC++ app window.
  aura::Window* arc_active_window_ = nullptr;

  // Maps active tasks to app id and package name.
  std::map<int, std::pair<std::string, std::string>> task_id_to_app_id_;

  // Set of tasks that have already rendered first frame.
  std::set<int> rendered_tasks_;

  // Maps tasks to most recent GFX jankiness results. Used for delta
  // calculation.
  std::map<std::string, mojom::GfxMetrics> package_name_to_gfx_metrics_;

  // Set of already reported ARC++ apps for the current session. Used to prevent
  // capturing too frequently.
  std::set<std::string> reported_categories_;

  // Keeps current active tracing session associated with |arc_active_window_|.
  std::unique_ptr<ArcAppPerformanceTracingSession> session_;

  // Callback to call when custom session is ready for testing.
  CustomSessionReadyCallback custom_session_ready_callback_;

  // Timer for jankiness tracing.
  base::OneShotTimer jankiness_timer_;

  // Used for automatic observer adding/removing.
  std::unique_ptr<exo::ScopedSurface> scoped_surface_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_H_
