# How to Add a Module in the Magic Stack

**Reference Document:** [go/magic-stack-adding-module-instruction](http://goto.google.com/magic-stack-adding-module-instruction)
*(For further questions, see the original design docs linked within the reference document.)*

This document outlines the architecture and steps required to add a new module to the Chrome Magic Stack on Android.

## Overview: What is the Magic Stack?
The **Magic Stack** is a horizontally scrollable container on Clank's New Tab Page that suggests relevant, dynamic modules to help users resume or accelerate their journeys.

From an architectural standpoint:
*   **`HomeModulesCoordinator`**: The central coordinator that owns the RecyclerView and manages the Magic Stack. It creates, shows, and hides modules based on rankings provided by the Segmentation Service. It acts as the `ModuleDelegate`.
*   **`ModuleProvider`**: The interface implemented by individual modules allowing the Magic Stack to control their visibility and lifecycle. Modules interact back with the Magic Stack using the `ModuleDelegate` API.

For the full detailed design, see the original design doc: [go/magic-stack-on-clank-dd](http://goto.google.com/magic-stack-on-clank-dd).

## Module Type Selection
Modules are categorized into two types:
*   **Stable Modules**: Designed for repeated user exposure (e.g., Single Tab, Price Check). Their ranking is based on freshness scores and user preference.
*   **Ephemeral Modules**: Displayed for a limited time based on specific triggers (e.g., promotional or educational tips). If the ephemeral module adheres to the Educational Tip layout, it is classified specifically as an Educational Tip ephemeral module.

No matter which type of module you are adding, you must first create a `ModuleType` in `ModuleDelegate`. See **Appendix: Creating a New Module Type** for all the places this new type must be registered.

---

## 1. Adding a New Standard Module

If your module is a stable module or an ephemeral module that does not use the educational tip layout, you need to create a complete Model-View-Coordinator (MVC) structure inside the `chrome/browser/magic_stack` or another appropriate directory.

### Required Components
1.  **Builder** (`ModuleProviderBuilder`, `ModuleConfigChecker`):
    *   Builds the module and its corresponding view.
    *   Instantiated and registered with the Magic Stack by `ChromeTabbedActivity`.
    *   If it is an ephemeral module, this class also sends signals (`InputContext`) to the magic stack.
2.  **Coordinator** (`ModuleProvider`):
    *   Instantiated by the builder.
    *   The Magic Stack controls visibility by sending show/hide commands to this class, and receives updates on readiness.
3.  **Mediator**:
    *   Contains the core business logic of the module.
4.  **View, Property, ViewBinder**:
    *   Manage the module's view updates based on mediator commands. Includes a `module_layout.xml`.
5.  **Unit Tests**: Add tests for your newly added MVC classes.

---

## 2. Adding an Educational Tip Card (Ephemeral)

Educational Tip modules use a shared MVC architecture to avoid duplication and rely on a Factory Design Pattern to configure specific cards.

### Steps to Add a New Card
1.  **Register the Module Type**: Add the new `ModuleType` inside `EducationalTipModuleUtils#getModuleTypes()`.
2.  **Create the Coordinator**: Create a coordinator that implements the `EducationalTipCardProvider` interface inside the `educational_tip/cards/` directory.
    *   This provides the card's specific content: title, description, image, and the action triggered by the "show me how" button.
    *   It is instantiated by the `EducationalTipCardProviderFactory`.
3.  **Unit Tests**: Add test cases for your card inside `EducationalTipModuleMediatorUnitTest`.

---

## 3. Segmentation Platform Integration (For Ephemeral Modules)

Ephemeral modules (both standard and educational tips) must be integrated into the Ephemeral Module Cards platform, which uses the Contextual Cards API to evaluate eligibility and ranking dynamically.

### Steps to Integrate (C++ Side)
1.  **`constants.h`**: Add the module name and the signal names used in the trigger scenarios.
2.  **`home_modules_card_registry.cc`**:
    *   Define the preference registration flags (used to track impressions and interactions via `NotifyCardShown` and `NotifyCardInteracted`).
    *   Implement the card creation logic inside `CreateAllCards()`.
3.  **Create `new_module.cc`**:
    *   Create a class implementing `CardSelectionInfo`.
    *   **`IsEnabled(int impression_count)`**: Checked during cold startup to verify feature flags and display limits.
    *   **`GetInputs()`**: Defines the signals and maps feature queries by `SignalKey`. Signals can come from C++ metrics or Java via `InputContext`.
    *   **`ComputeCardResult(const CardSelectionSignals& signals)`**: Dynamically checks real-time signals and user interaction history to return the card's rendering position (`EphemeralHomeModuleRank::kTop`, `kLast`, or `kNotShown`).

### Java Signal Integration
If your `GetInputs()` implementation requires signals passed in as an `InputContext`:
*   For **Educational Tips**: Add the functions inside `EducationalTipCardProviderSignalHandler`. They will be triggered by its `createInputContext()` method.
*   For **Standard Ephemeral Modules**: Add the gathering logic inside the `createInputContext()` function you created in your custom module Builder (from Section 1).

### Unit Tests
Don't forget to add C++ and Java unit tests covering your integration logic:
*   `home_modules_card_registry_unittest.cc`
*   Your `new_module_unittest.cc`
*   `EducationalTipCardProviderSignalHandlerUnitTest.java` (if applicable)

---

## Appendix: Creating a New Module Type

The `ModuleType` enum is utilized across configuration settings and metrics. When you create a new module type in `ModuleDelegate.ModuleType`, you must consistently register it in the following places:

1.  `HomeModulesConfigSettings:getTitleForModuleType`
2.  `HomeModulesMetricsUtils:getModuleName`
3.  `HomeModulesMetricsUtils:convertLabelToModuleType`
4.  `tools/metrics/histograms/metadata/magic_stack/histograms.xml` (Update `ModuleType` enums)
5.  `tools/metrics/histograms/metadata/magic_stack/enums.xml` (Update `ModuleType` enums)

**If the module is an Educational Tip, also update:**
6.  `HomeModulesUtils:sEducationalTipCardList`
7.  `EducationalTipCardProviderSignalHandler:createInputContext`
