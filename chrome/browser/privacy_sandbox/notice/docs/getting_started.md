# Getting Started with the Notice Framework

This guide provides a step-by-step tutorial for adding a new user-facing notice to Chrome using the Notice Framework.

## Before You Begin: The Core Concept - APIs vs. Notices

To use the framework, you must understand its core design principle: the separation of a feature's abstract requirements (`NoticeApi`) from the concrete UI that is shown to the user (`Notice`).

*   **A `NoticeApi` is the "Why."** It represents an abstract feature or permission that requires user acknowledgment or explicit consent. For example, "Ad Measurement API." It is **not** a UI element. It has eligibility rules that determine if a user should even be considered for it.

*   **A `Notice` is the "What" and "How."** It is the actual, user-facing UI element that gets displayed, such as a modal dialog or a bubble. A `Notice` is linked to one or more `NoticeApi`s that it "fulfills."

**The Relationship:** The framework's primary job is to figure out which `NoticeApi`s a user is eligible for but has not yet had "fulfilled." It then selects the best `Notice` (the UI) to show the user to achieve that fulfillment.

This separation is powerful. It allows one abstract feature (`NoticeApi`) to be handled by many different UIs (`Notice`s) depending on the context, such as the user's platform, region, or other notices they've seen in the past.

---
## Adding a new notice

### Step 1: Define the Notice Identifier

Every new notice needs a unique identifier. This is defined as an enum value in the framework's central Mojo interface.

**File**: `chrome/browser/privacy_sandbox/notice/notice.mojom`

Add your new notice to the `PrivacySandboxNotice` enum.

**Example:**
```mojom
// An identifier for the different notices.
enum PrivacySandboxNotice {
  kTopicsConsentNotice = 0,
  kProtectedAudienceMeasurementNotice = 1,
  kThreeAdsApisNotice = 2,
  kMeasurementNotice = 3,
  kMyCoolFeatureNotice = 4, // Add your new notice here
};
```

### Step 2: Define Feature Flags

Every `NoticeApi` and every specific `Notice` instance must be controlled by a `base::Feature` flag.

**Files**:
*   `chrome/browser/privacy_sandbox/notice/notice_definitions.h` (declarations)
*   `chrome/browser/privacy_sandbox/notice/notice_definitions.cc` (definitions)

**Example:**

1.  **Declare the flags in `.h`:**
    ```cpp
    // My Cool Feature API
    BASE_DECLARE_FEATURE(kNoticeFrameworkMyCoolFeatureApiFeature);

    // My Cool Feature Notice on Desktop
    BASE_DECLARE_FEATURE(kMyCoolFeatureNoticeDesktopFeature);
    ```

2.  **Define the flags in `.cc`:**
    ```cpp
    // My Cool Feature API
    BASE_FEATURE(kNoticeFrameworkMyCoolFeatureApiFeature,
                 "PSNoticeFrameworkMyCoolFeatureApi",
                 base::FEATURE_DISABLED_BY_DEFAULT);

    // My Cool Feature Notice on Desktop. The feature name ("MyCoolFeatureNoticeDesktop")
    // is also used as the storage key in Prefs.
    BASE_FEATURE(kMyCoolFeatureNoticeDesktopFeature,
                 "MyCoolFeatureNoticeDesktop",
                 base::FEATURE_DISABLED_BY_DEFAULT);
    ```

### Step 3: Register the `NoticeApi`

Define the abstract requirement for your feature in the `NoticeCatalog`.

**File**: `chrome/browser/privacy_sandbox/notice/notice_catalog.cc`

Inside the `NoticeCatalogImpl::Populate()` method, register your `NoticeApi`. The `SetEligibilityCallback` is used for any additional, feature-specific eligibility logic that goes beyond the standard checks performed by the framework.

**Example:**
```cpp
  // In NoticeCatalogImpl::Populate()
  NoticeApi* my_cool_feature_api =
      RegisterAndRetrieveNewApi()
          ->SetFeature(&kNoticeFrameworkMyCoolFeatureApiFeature)
          ->SetEligibilityCallback(EligibilityCallback(
              &MyCoolFeatureService::IsEligibleForMyCoolFeature));
```

### Step 4: Choose a Surface and Register the `Notice`

A "Surface" represents a moment or a location in the Chrome UI where a notice can appear. You must decide where your notice should be displayed.

First, check for an existing `SurfaceType` in `chrome/browser/privacy_sandbox/notice/notice_definitions.h`. If a suitable one doesn't exist, you can add a new one.

Next, define the `Notice` that will fulfill your `NoticeApi` on that surface.

**File**: `chrome/browser/privacy_sandbox/notice/notice_catalog.cc`

Still inside `NoticeCatalogImpl::Populate()`, register your notice.

**Example (for a single notice):**
```cpp
  // In NoticeCatalogImpl::Populate()
  RegisterAndRetrieveNewNotice(
      &Make<Notice>, // Or &Make<Consent> for consent-based flows
      {kMyCoolFeatureNotice, kDesktopNewTab})
          ->SetFeature(&kMyCoolFeatureNoticeDesktopFeature)
          ->SetTargetApis({my_cool_feature_api});
```

### Step 5: Implement the UI

The framework is now aware of your notice. The final step is to implement the UI and connect it to the `NoticeService`.

Your approach will depend on whether you are using an existing surface or creating a new one:

*   **Using an Existing Surface (e.g., `kDesktopNewTab`)**: The existing `DesktopViewManager` already monitors this surface. When it becomes active, it calls `NoticeService::GetRequiredNotices()` and will discover your notice if it's eligible. The `DesktopViewManager` will then attempt to instantiate your UI, typically by calling a static `Show()` method on your view class. Your UI must be compatible with this instantiation pattern.

*   **Creating a New Surface**: If you've defined a new `SurfaceType`, you are responsible for creating the UI management logic. This logic must:
    1.  Detect when your new surface is active.
    2.  Call `NoticeService::GetRequiredNotices(kYourNewSurfaceType)` to get the list of notice IDs to display.
    3.  Display the corresponding UI for each notice in the returned list.

Regardless of the approach, all UI implementations have one critical responsibility: reporting user interactions back to the framework.

**Connecting the UI to the `NoticeService`**

Your UI code must call `NoticeService::EventOccurred()` to inform the framework of user actions. This is the primary way the framework tracks notice fulfillment.

**Example (Conceptual UI logic):**
```cpp
// In your UI's entry point or handler.
void ShowMyNoticeFlow(Profile* profile, SurfaceType surface) {
  auto* notice_service =
      PrivacySandboxNoticeServiceFactory::GetForProfile(profile);

  // 1. Get the required notices for the current surface.
  std::vector<notice::mojom::PrivacySandboxNotice> required_notices =
      notice_service->GetRequiredNotices(surface);

  if (required_notices.empty()) {
    return;
  }

  // 2. Display the UI for the first notice in the sequence.
  // This could involve creating a dialog, navigating to a WebUI page, etc.
  DisplayNoticeUI(profile, required_notices.front());
}

// In your UI's button handler or event callback.
void OnMyNoticeActionButtonClicked(
    Profile* profile,
    std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType> notice_id) {
  auto* notice_service =
      PrivacySandboxNoticeServiceFactory::GetForProfile(profile);

  // 3. Report the user's action back to the service.
  notice_service->EventOccurred(notice_id,
      notice::mojom::PrivacySandboxNoticeEvent::kAck);

  // The UI can now be closed. If it were part of a multi-step flow,
  // the managing logic would now call GetRequiredNotices() again to see
  // if another notice needs to be shown.
}
```

### Step 6: Update Histograms

To prevent test failures, you must add your new notice to the list of known notices for UMA histograms.

**File**: `tools/metrics/histograms/metadata/privacy/histograms.xml`

Find the `<variants name="PSNotice">` section and add a new `<variant>` entry. The `name` of the variant **must match the string name of the `base::Feature`** you defined for the notice in Step 2.

**Example:**
```xml
<variants name="PSNotice">
  ...
  <variant name="MyCoolFeatureNoticeDesktop" summary="The 'My Cool Feature' notice on desktop."/>
  ...
</variants>
```

By adding your notice to this list, the framework will automatically emit several histograms. Key metrics include:

*   `PrivacySandbox.Notice.NoticeEvent.{PSNotice}`: Records each user interaction (e.g., `Shown`, `Ack`, `OptIn`) with the notice as it happens.
*   `PrivacySandbox.Notice.Startup.LastRecordedEvent.{PSNotice}`: Records the very last event that occurred for a notice, captured once during browser startup. This helps track the final state of the notice across sessions.