# Notice Framework - Architecture Deep Dive

This document provides a detailed technical overview of the components within the Privacy Sandbox Notice Framework. It is intended for developers who need a deeper understanding of how the framework operates internally.

## Core Components

The framework is composed of several key C++ classes that work together to manage the notice lifecycle.

### `NoticeService` (The Orchestrator)

*   **Location**: `chrome/browser/privacy_sandbox/notice/notice_service.h`
*   **Role**: This is the central brain of the framework. It is a `ProfileKeyedService` that orchestrates the entire process of selecting and displaying a notice.

**Key Responsibilities:**

1.  **Determines Required Notices**: The core logic resides in `GetRequiredNotices(SurfaceType surface)`. This method executes a selection algorithm to determine the precise sequence of notices to show:
    *   **a. Filter APIs**: It first identifies all `NoticeApi`s that are currently enabled, eligible for the user, and have not already been fulfilled.
    *   **b. Filter Notices**: It then finds all `Notice`s that are registered for the given `SurfaceType`, are enabled, have not been fulfilled, and can satisfy all of their target APIs' eligibility requirements (e.g., a `Consent` notice for an API requiring consent). Crucially, it only considers notices whose target APIs are all present in the filtered list from the previous step.
    *   **c. Group and Score**: Eligible notices are grouped together (either by an explicit `NoticeViewGroup` or as a group of one). It then uses a scoring algorithm (`CompareNoticeGroups`) to select the best group. The scoring prioritizes groups that fulfill the most APIs with the fewest number of separate UI interactions.
    *   **d. Sort and Return**: The notices within the winning group are sorted by their defined display order, and their IDs are returned.
2.  **Provides Notices for Display**: The various `ViewManager` implementations for the different surfaces (e.g., `DesktopViewManager`) call the `NoticeService` to retrieve the list of required notices. The `ViewManager` is then responsible for initiating the display of the UI.
3.  **Processes Events**: It exposes the `EventOccurred` method. `ViewManager`s call this to report user interactions, which the service then records in `NoticeStorage` and uses to update the fulfillment status of the relevant `NoticeApi`s.

### `NoticeCatalog`

*   **Location**: `chrome/browser/privacy_sandbox/notice/notice_catalog.h`
*   **Role**: A central, in-memory registry of all known `NoticeApi`s and `Notice`s.

**Key Responsibilities:**

*   **Registration**: The `NoticeCatalogImpl::Populate()` method is where all `NoticeApi`s and `Notice`s are defined and their relationships are established. This includes setting target APIs, prerequisite APIs, `base::Feature` flags, and eligibility callbacks.
*   **Provides Data to `NoticeService`**: It serves as the single source of truth for the `NoticeService`'s decision-making process.

### `NoticeStorage`

*   **Location**: `chrome/browser/privacy_sandbox/notice/notice_storage.h`
*   **Role**: An abstraction layer for persistent storage.

**Key Responsibilities:**

*   **Manages Prefs**: The `PrivacySandboxNoticeStorage` implementation handles all reading and writing to the profile's `PrefService` under the `privacy_sandbox.notices` dictionary.
*   **Handles Data Model**: It manages the schema for storing notice history, including the event type and timestamp.
*   **Records Histograms**: It is responsible for writing the standard set of UMA metrics for notice events.

### `ViewManager`s (e.g., `DesktopViewManager`)

*   **Location**: `chrome/browser/privacy_sandbox/notice/desktop_view_manager.h`
*   **Role**: Platform-specific components that bridge the gap between the abstract framework logic and the concrete UI implementation.

**Key Responsibilities:**

*   **UI Lifecycle Management**: The `NoticeService` provides the `ViewManager` with a list of required notices. The `ViewManager` is responsible for showing the first notice, and then advancing through the list as user actions are performed. It manages a `pending_notices_to_show_` list to track the sequence.
*   **Handles Entry Points**: The `ViewManager` owns handlers that detect when a notice flow should be initiated. For example, `DesktopViewManager` owns `NavigationHandler`, which monitors navigations to specific chrome-owned pages (like the New Tab Page or Settings) before triggering the notice flow.
*   **Event Communication**: It provides an `OnEventOccurred` method that the UI code calls to report user interactions. The `ViewManager` forwards this event to the `NoticeService`. Based on the event (e.g., `kAck`, `kOptIn`), it will then advance its internal state and notify the active UI view to either display the next notice in the sequence or close itself. This notification is handled via an `Observer` pattern, where the UI views are the observers.

## The Concept of a "Surface"

A `SurfaceType` is a key concept that enables the framework to be platform-agnostic.

*   **Definition**: It is an enum that represents a specific context or location in the UI where a notice can be shown (e.g., `kDesktopNewTab`).
*   **Filtering**: The `NoticeService` takes a `SurfaceType` as a parameter when it starts its evaluation. The `NoticeCatalog` is then filtered to only consider `Notice`s that are registered for that specific surface.
*   **Extensibility**: To add a new surface, a developer would need to:
    1.  Add a new value to the `SurfaceType` enum.
    2.  Implement a new entry point handler (similar to `NavigationHandler`) that can detect when that surface is active.
    3.  Wire this new handler into the appropriate `ViewManager`.
    4.  Register `Notice`s in the catalog for the new surface type.