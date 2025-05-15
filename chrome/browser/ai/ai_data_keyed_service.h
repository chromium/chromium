// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_
#define CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/common/actor.mojom-forward.h"
#endif

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

// Browser service to collect AI data, including data resulting from triggering
// actor tasks.
class AiDataKeyedService : public KeyedService {
 public:
  // Data related to AiData collection.
  using BrowserData = optimization_guide::proto::BrowserCollectedInformation;
  using AiData = std::optional<BrowserData>;
  using AiDataCallback = base::OnceCallback<void(AiData)>;
  using AiDataSpecifier =
      optimization_guide::proto::ModelPrototypingCollectionSpecifier;

  explicit AiDataKeyedService(content::BrowserContext* browser_context);
  AiDataKeyedService(const AiDataKeyedService&) = delete;
  AiDataKeyedService& operator=(const AiDataKeyedService&) = delete;
  ~AiDataKeyedService() override;

  // Returns the list of extensions that are allowlisted for data collection.
  static bool IsExtensionAllowlistedForData(const std::string& extension_id);

  // Returns the list of extensions that are allowlisted for actions.
  static bool IsExtensionAllowlistedForActions(const std::string& extension_id);

  // Returns whether an extension is allowed to run on stable channel.
  static bool IsExtensionAllowlistedForStable(const std::string& extension_id);

  // Fills an AiData and returns the result via the passed in callback. If the
  // AiData is empty, data collection failed. |callback| is guaranteed to be
  // called, and guaranteed to be called asynchronously. This method uses a set
  // of default specifiers. See |GetAiDataWithSpecifiers| for more details.
  void GetAiData(int dom_node_id,
                 content::WebContents* web_contents,
                 std::string user_input,
                 AiDataCallback callback,
                 int tabs_for_inner_text = 10);

  // Fills an AiData and returns the result via the passed in callback. If the
  // AiData is empty, data collection failed. |callback| is guaranteed to be
  // called, and guaranteed to be called asynchronously.
  // |tabs_for_inner_text|: The number of tabs to collect inner text for.
  void GetAiDataWithSpecifier(content::WebContents* web_contents,
                              AiDataSpecifier specifier,
                              AiDataCallback callback);

  // Starts an actor task.
  void StartTask(
      optimization_guide::proto::BrowserStartTask task,
      base::OnceCallback<
          void(optimization_guide::proto::BrowserStartTaskResult)> callback);

  // Stops an actor task.
  void StopTask(int64_t task_id, base::OnceCallback<void(bool)> callback);

  // Executes an actor action. The first action in a task must be navigate.
  void ExecuteAction(
      optimization_guide::proto::BrowserAction action,
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback);

  // Returns true if the associated ActorCoordinator is active on the given
  // `tab`. This can be used by callers to customize certain behaviour that
  // might interfere with the ActorCoordinator.
  bool IsActorCoordinatorActingOnTab(const content::WebContents* tab) const;

  static const base::Feature& GetAllowlistedAiDataExtensionsFeatureForTesting();
  static const base::Feature&
  GetAllowlistedActionsExtensionsFeatureForTesting();

 private:
#if BUILDFLAG(ENABLE_GLIC)
  // Called when the actor coordinator has created a new tab for the task.
  void TaskCreated(
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback,
      optimization_guide::proto::BrowserAction action,
      int task_id,
      int tab_id,
      base::WeakPtr<tabs::TabInterface>);
  // Converts the result of a page context fetch to a BrowserActionResult.
  void ConvertToBrowserActionResult(
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback,
      int task_id,
      int tab_id,
      actor::mojom::ActionResultPtr action_result,
      glic::mojom::GetContextResultPtr result);
  // Called when the actor coordinator has started a tas.
  void OnTaskCreated(
      base::OnceCallback<
          void(optimization_guide::proto::BrowserStartTaskResult)> callback,
      int task_id,
      int tab_id,
      base::WeakPtr<tabs::TabInterface> tab);
  // Called when the actor coordinator has finished an action which required
  // task creation.
  void OnActionFinished(
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback,
      int task_id,
      int tab_id,
      actor::mojom::ActionResultPtr action_result);
  // The actor coordinator which manages task and action routing.
  std::unique_ptr<actor::ActorCoordinator> actor_coordinator_;

  // Whether the task still needs a navigate action to be executed as the
  // first action in the task.
  bool task_needs_navigate_ = false;

  // The tab that the task is running on.
  base::WeakPtr<tabs::TabInterface> tab_;

  // The tab id and task id of the current task.
  int tab_id_ = 1;
  int task_id_ = 1;
#endif

  // A `KeyedService` should never outlive the `BrowserContext`.
  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<AiDataKeyedService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_
