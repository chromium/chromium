# Critical User Journeys (CUJ) Framework

## Introduction

The Critical User Journey (CUJ) framework provides a structured way to measure and monitor multi-step user tasks in Chromium. While traditional UMA histograms are excellent for tracking discrete events, they often struggle to represent the flow and success rate of complex, time-dependent task sequences.

The CUJ framework addresses this by allowing developers to define a sequence of interaction steps that represent a complete user task. It is built strictly on top of the [ui::InteractionSequence](/ui/base/interaction/README.md) library, which provides the underlying logic for tracking UI elements (via [ui::ElementIdentifier](/ui/base/interaction/README.md#Named-elements)) and observing events. By leveraging `ui::InteractionSequence`, the framework can reuse the same primitives and patterns used in modern Chromium [interactive UI tests (Kombucha)](/chrome/test/interaction/README.md), making it easier for developers to instrument their features with high-quality metrics.

The primary goals of this framework are:
1.  **Observability:** To provide clear signals on where users drop off in a multi-step task.
2.  **Automation:** To automatically log completion, failure, and step-level metrics (e.g., `{JourneyName}.StepReached`) without requiring repetitive boilerplate.
3.  **Lifecycle Management:** To handle the registration and tracking of journeys through a unified `CriticalUserJourneyService`, ensuring proper cleanup and resource management via `KeyedService`.

## System Architecture

The CUJ framework is designed around four primary components that manage the lifecycle of a journey from definition to metric logging.

### 1. CriticalUserJourneyService (KeyedService)
The `CriticalUserJourneyService` is the central orchestrator of the framework. As a `KeyedService` owned by the `Profile`, it ensures that journey tracking is scoped to the correct user profile and tied to its lifecycle. Its responsibilities include:
*   **Initialization:** Bootstrapping the registry and subscribing to the initial triggers for all registered journeys.
*   **Session Management:** Instantiating and owning `CriticalUserJourneySession` objects when a journey's starting trigger is detected.
*   **Cleanup:** Ensuring active sessions are terminated and resources are freed when the profile is destroyed.

### 2. CriticalUserJourneyRegistry
The `CriticalUserJourneyRegistry` serves as the single source of truth for all journey *definitions*. During service initialization, journeys are registered here. The registry allows the service to efficiently look up which journey should be started when a specific `ui::ElementIdentifier` or event is observed in the UI.

### 3. CriticalUserJourney (The Definition)
A `CriticalUserJourney` is a static "blueprint" for a specific user task. It is created using a `Builder` pattern and defines:
*   **Feature Binding:** Every journey must be associated with a `base::Feature`. This provides a mandatory kill switch and ensures the journey name is consistently derived from the feature's string name.
*   **Steps:** The sequence of UI interactions (elements, events, types) that constitute the journey.
*   **Metadata:** Optional integration points like Happiness Tracking Surveys (HaTS).
*   **Branching Logic:** Complex journeys can use `AddAnyOf` to handle multiple valid paths at a given step.

### 4. CriticalUserJourneySession (The Active Instance)
While a `CriticalUserJourney` defines the *what*, a `CriticalUserJourneySession` represents the *now*. For every active user task, a session is created to:
*   **Gated Execution:** The session only runs if the journey's associated feature flag is enabled.
*   **Track Progress:** It encapsulates a `ui::InteractionSequence` built from the journey's blueprint.
*   **Handle Timeouts:** It manages timers to ensure that stale or abandoned journeys do not leak resources or skew metrics.
*   **Log Results:** Upon completion or failure, the session reports the outcome (Succeeded, Aborted, or Timed Out) back to the service for final metric recording.

## Automated Histogram Logging

The CUJ framework automatically generates and logs several UMA histograms for each registered journey. These metrics are prefixed with `CriticalUserJourney.{JourneyName}.`, where `{JourneyName}` is the string name of the `base::Feature` associated with the journey.

### `{JourneyName}.StepReached`
*   **Type:** Sparse Histogram
*   **Description:** Logs the `metric_id` of each step as the user successfully reaches it.
*   **Usage:** This histogram provides a "funnel" view of the journey, allowing developers to see how many users progress through each stage of the task.

### `{JourneyName}.StepAborted`
*   **Type:** Sparse Histogram
*   **Description:** Logs the `metric_id` of the last reached step when a journey is aborted or times out.
*   **Usage:** This is critical for identifying exactly *where* users are dropping off or encountering friction in the journey.

### `{JourneyName}.Result`
*   **Type:** Enumerated Histogram (`CriticalUserJourneyResult` enum)
*   **Description:** Logs the final outcome of the journey session.
*   **Values:**
    *   `kCompleted` (0): The user successfully reached the final step of the journey.
    *   `kAborted` (1): The journey was terminated before completion (e.g., the user closed the relevant UI or navigated away).
    *   `kTimeout` (2): The journey exceeded its defined `time_out_duration` at a particular step.

## Best Practices

### UI-Driven Sequences vs. Simple Action Logging
The CUJ framework is specifically designed for tracking **UI-driven sequences** that depend on `ui::InteractionSequence`. It should not be used for simple, disconnected action logging where traditional UMA histograms or UserAction logging are more efficient.

*   **Use the CUJ Framework when:**
    *   The user task involves a sequence of specific UI interactions (e.g., opening a menu, then selecting a specific sub-item, then interacting with the resulting dialog).
    *   The task has a clear "happy path" and defined success/failure states.
    *   You need to measure the success rate and identifying specific drop-off points in a multi-step process.
    *   Each step in the journey can be identified by an `ui::ElementIdentifier` tied to some action (pressed, activated, shown / hidden, custom events).
*   **Use Simple Action Logging (UMA/UserActions) when:**
    *   You only need to track a single, discrete user action (e.g., "User clicked the 'Settings' button").
    *   The events are independent and can occur in any order without a shared task context.
    *   There is no requirement to track the temporal or sequential relationship between multiple UI states.

### Keep Journeys Concise
To maintain high-quality data signals, journeys should be focused on a single, well-defined user task. Overly complex journeys with excessive branching (`AddAnyOf`) can become difficult to analyze and may lead to "noisy" metrics. If a journey feels too large, consider whether it can be decomposed into smaller, more focused sub-journeys.

### UI Element Persistence and Visibility
A common pitfall is defining a journey step for an element that may be destroyed or hidden before the `ui::InteractionSequence` can observe it.

Ensure that the elements you are tracking are persistent enough for the sequence to transition through them. If an interaction causes a UI element to be replaced (e.g., navigating to a new page), ensure the journey can account for this by updating the step to use an element that will become present in the replaced step or by listening for a custom event that you emit that is not tied to a UI element (i.e. a download starts / finishes, page transition, etc).

## Troubleshooting

### Timeouts
Every journey defined in the CUJ framework has an associated `time_out_duration` (either a default or one explicitly set during the journey's construction). If a user does not reach the next step in the sequence within this timeframe, the journey session will automatically transition to a `kTimeout` state and terminate.

Common causes for timeouts include:
*   **User Inactivity:** The user starts a task but stops or switches to another application before completing it.
*   **Unexpected UI States:** If a UI element or event that the journey expects never appears, the session will eventually time out. This often indicates a bug in the feature's UI logic or a mismatch between the journey's definition and the actual implementation.
*   **Performance Issues:** Significant delays in UI rendering or event processing can cause the journey to exceed its timeout duration even if the user is actively attempting to complete the task.

### UI State Mismatches and Aborted Journeys
A journey will transition to the `kAborted` state if the underlying `ui::InteractionSequence` is terminated before reaching the final step. This typically happens when the UI elements being tracked are destroyed or become hidden unexpectedly.

Common scenarios leading to aborted journeys:
*   **Element Destruction:** If a UI component (e.g., a dialog or menu) that contains a tracked `ui::ElementIdentifier` is closed or destroyed while the journey is in progress, the sequence will abort.
*   **Navigation Events:** Navigating away from a page or closing a tab that is part of the journey's context will cause any active sessions associated with that context to terminate.
*   **Focus Requirements:** Some `ui::InteractionSequence` steps may point to an element that is in a bubble which auto-dismisses on loss of focus / when it is no longer the active window causing the journey to abort early.

## Step-by-Step Implementation Guide

Follow these steps to instrument a new Critical User Journey in Chromium.

### 1. Define UI Element Identifiers
The CUJ framework relies on `ui::ElementIdentifier` to track UI elements. If your feature's UI components don't already have identifiers, define them in your controller or feature class using `DECLARE/DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE()` or in [chrome/browser/ui/browser_element_identifiers.h](/chrome/browser/ui/browser_element_identifiers.h) if it is a top level UI element.

See [ui/base/interaction/element_identifier.h](/ui/base/interaction/element_identifier.h) for more information.

Ensure these identifiers are assigned to the actual UI views using `views::View::SetProperty(views::kElementIdentifierKey, kYourFeatureMainButtonId)`.

### 2. Define the Journey Feature Flag
Every journey requires a dedicated `base::Feature` to serve as its identity and kill switch. Define this in `chrome/browser/metrics/critical_user_journeys/features.h`:

```cpp
BASE_DECLARE_FEATURE(kMyFeatureJourney);
```

And in `chrome/browser/metrics/critical_user_journeys/features.cc`:

```cpp
BASE_FEATURE(kMyFeatureJourney, "MyFeatureJourney", base::FEATURE_ENABLED_BY_DEFAULT);
```

### 3. Create the Journey Definition
Define your journey using the `CriticalUserJourney::Builder`. You must pass a pointer to your journey's `base::Feature` to the constructor.

```cpp
std::unique_ptr<metrics::CriticalUserJourney> CreateMyFeatureJourney() {
  return metrics::CriticalUserJourney::Builder(&kMyFeatureJourney)
      .AddStep(kYourFeatureMainButtonId, ui::InteractionSequence::StepType::kActivated, 1)
      .AddStep(kYourFeatureDialogId, ui::InteractionSequence::StepType::kShown, 2)
      // Add more steps as needed...
      .Build();
}
```

### 4. Register the Journey
Register your journey via `CriticalUserJourneyRegistry::AddJourneys`. Doing so allows all external dependencies to bubble into a single location. The service will automatically skip registration if the journey's feature flag is disabled.

```cpp
void CriticalUserJourneyRegistry::AddJourneys() {
  AddJourney(CreateMyFeatureJourney());
  // ... other registrations
}
```

### 5. Trigger the Journey
A journey starts when its first defined step is observed by the framework. Ensure that the initial `ui::ElementIdentifier` or event is correctly triggered by user interaction. The `CriticalUserJourneyService` automatically listens for these starting triggers once the journey is registered.

### 6. Register Enums / Histograms in XML
You must define the steps in `tools/metrics/histograms/metadata/critical_user_journeys/enums.xml` in order to have proper labels when viewing the metrics.

```xml
<enum name="MyFeatureJourneySteps">
  <int value="1" label="Press the first button"/>
  <int value="2" label="Click a different button"/>
  <int value="3" label="Wait for dialog to show"/>
</enum>
```

Finally, you must register the generated histograms in `tools/metrics/histograms/metadata/critical_user_journeys/histograms.xml`.

```xml
<histogram name="CriticalUserJourney.MyFeatureJourney.{StepAction}" enums="MyFeatureJourneySteps" expires_after="2026-03-31">
  <owner>your-ldap@chromium.org</owner>
  <summary>
    The steps reached in the {JourneyName} critical user journey.
  </summary>
  <token key="StepAction" variants="CriticalUserJourneyStepAction"/>
</histogram>
```

And then add your journey to the results metric to track the final outcomes of the journeys. Use the `<variant>` tag to efficiently define the metrics for your journey.

```xml
<histogram name="CriticalUserJourney.{JourneyName}.Result" enum="CriticalUserJourneyResult" expires_after="2026-03-31">
  <owner>your-ldap@chromium.org</owner>
  <summary>
    Records the final outcome (Completed, Aborted, or Timed out) of the
    {JourneyName} critical user journey.
  </summary>
  <token key="JourneyName">
    <variant name="OtherFeatureJourney"/>
    <variant name="AnotherFeatureJourney"/>
    <variant name="MyFeatureJourney"/> <!-- Add it here! -->
  </token>
</histogram>
```

Please consider using `IfThisThenThat Lint` to keep your journey and enum in sync! Doing so prevents misleading information in the metrics during metric analysis.

See [documentation](https://www.chromium.org/chromium-os/developer-library/guides/development/keep-files-in-sync/) for more details.

## Examples

### Linear Journey
A simple linear journey tracks a fixed sequence of user actions.

```cpp
std::unique_ptr<metrics::CriticalUserJourney> CreateSettingsChangeJourney() {
  return metrics::CriticalUserJourney::Builder(&kSettingsChangeJourneyFeature)
      // Step 1: User opens the main menu.
      .AddStep(kMainMenuButtonId, ui::InteractionSequence::StepType::kActivated, 1)
      // Step 2: User navigates to the Settings page.
      .AddStep(kSettingsMenuEntryId, ui::InteractionSequence::StepType::kActivated, 2)
      // Step 3: The Settings dialog is shown to the user.
      .AddStep(kSettingsDialogId, ui::InteractionSequence::StepType::kShown, 3)
      // Step 4: User clicks "Save" to commit their changes.
      .AddStep(kSettingsSaveButtonId, ui::InteractionSequence::StepType::kActivated, 4)
      .Build();
}
```

### Complex Journey with Branching
The `AddAnyOf` method allows a journey to proceed if any one of several defined paths is taken.

```cpp
std::unique_ptr<metrics::CriticalUserJourney> CreateMultiOptionTaskJourney() {
  return metrics::CriticalUserJourney::Builder(&kMultiOptionTaskJourneyFeature)
      // Start by opening the selection interface.
      .AddStep(kOpenSelectorButtonId, ui::InteractionSequence::StepType::kActivated, 1)
      // The user can choose between two different options to proceed.
      .AddAnyOf({
          metrics::Branch(kOptionAButtonId, ui::InteractionSequence::StepType::kActivated, 2),
          metrics::Branch(kOptionBButtonId, ui::InteractionSequence::StepType::kActivated, 3)
      })
      // Final confirmation step regardless of which option was chosen.
      .AddStep(kConfirmationDialogId, ui::InteractionSequence::StepType::kShown, 4)
      .Build();
}
```

## Happiness Tracking Surveys (HaTS) Integration

The CUJ framework supports triggering a [Happiness Tracking Survey (HaTS)](/chrome/browser/ui/hats/README.md) automatically upon the successful completion of a journey. This allows for gathering qualitative user feedback immediately after they have finished a key task.

To enable HaTS integration, use the `LaunchHatsSurveyOnCompletion` method in the `CriticalUserJourney::Builder`. You must provide a `metrics::HatsParams` struct containing the required trigger and optional product-specific data.

```cpp
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"

std::unique_ptr<metrics::CriticalUserJourney> CreateJourneyWithHats() {
  metrics::HatsParams hats_params;
  // The trigger string associated with your survey in the HaTS console.
  hats_params.trigger = "your-hats-trigger-string";
  // Optional: Provide product-specific data to be sent with the survey response.
  hats_params.product_specific_string_data = {{"feature_version", "1.0"}};

  return metrics::CriticalUserJourney::Builder(&kFeatureJourneyWithHatsFeature)
      .AddStep(kFeatureStartButtonId, ui::InteractionSequence::StepType::kActivated, 1)
      .AddStep(kFeatureCompleteDialogId, ui::InteractionSequence::StepType::kShown, 2)
      .LaunchHatsSurveyOnCompletion(std::move(hats_params))
      .Build();
}
```

