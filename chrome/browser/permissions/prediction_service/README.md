# Permissions Prediction Service (`//chrome/browser`)

## Overview

This directory contains the browser-specific implementation of the permissions
prediction service. Its primary goal is to predict whether a permission request
is likely to be granted and, based on that prediction, decide whether to show a
standard permission prompt or a quieter UI variation (a permission prompt that
is much less prominent). This helps in reducing the number of permission
prompts that are perceived as intrusive by clients.

The classes in this folder use or implement the platform-agnostic components
defined in
[`//components/permissions/prediction_service/README.md`][components_readme].

## Architecture

The permissions prediction service is architecturally split into two main
layers: platform-agnostic base classes and model handlers in `//components` and a
browser-specific implementation layer here in `//chrome/browser`.

- **`//components/permissions/prediction_service`**: Contains the low-level logic
  for loading and executing TFLite models, the client for the remote prediction
  service, and fundamental abstractions like `PermissionUiSelector`.

- **`//chrome/browser/permissions/prediction_service`**: Uses browser specific
  infrastructure and classes defined in the components layer. It is responsible for:

  - Implementing the concrete [`PermissionUiSelector`][ui_selector] interfaces.
  - Gathering browser-specific features for the models (e.g., page language,
    snapshots, text embeddings).
  - Interacting with `Profile`-keyed services like `OptimizationGuideKeyedService`.

When a permission request is initiated, the
[`PermissionRequestManager`][request_manager] consults its configured UI selectors to determine
whether to show a normal prompt or a quiet UI. This directory contains two such
selectors that work in parallel; they are described in the components section below.

## Components

The following components are defined in this directory:

### UI Selectors

- **[`PermissionsAiUiSelector`][prediction_based_ui_selector]**: A ML-based selector that decides
  whether a quiet permission prompt should be shown for a geolocation or
  notifications permission request. For this decision it gathers various aggregated
  client-specific and contextual signals (features) and queries one or more
  prediction models (both on-device and server-side) to get a grant
  likelihood. It also contains the logic to decide which of those models to use.
- **[`ContextualNotificationPermissionUiSelector`][contextual_ui_selector]**: Provides a rule-based
  check for notification permissions. It acts as a blocklist-style
  mechanism, using Safe Browsing verdicts to decide if the site is known as
  showing abusive, disruptive, or unsolicited prompts.

### Prediction Models and Handlers

The service uses a combination of a remote service and various on-device models
to generate predictions. ModelHandler classes are used to trigger model download and
initialization and provide an interface for model execution.G

#### Model Access

- **[`PredictionModelHandlerProvider`][model_handler_provider]**: Acts as a factory that provides the
  `PermissionsAiUiSelector` with the correct on-device model
  handler for a given permission request type.

#### Prediction Models

The service can leverage several types of prediction models with varying inputs,
operating in different workflows. All models - except for CPSSv1 - are **only
active if the user has enabled "Make searches and browsing better
(`unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled`)"**

- **Server-side CPSS (Chrome Permissions Suggestion Service)**: A remote service
  that provides a grant likelihood prediction. It receives a combination of
  aggregated client signals (e.g., aggregated counts of the client's past
  permission decisions), contextual signals (e.g., gesture type, relevance
  score from on-device models), and URL-specific signals (the requesting
  origin). Communication is handled by the
  [`PredictionServiceRequest`][prediction_service_request] class.

- **On-device Models**: For privacy and latency benefits, several on-device
  models are used. The logic for gathering the inputs for each model resides in
  this `//chrome/browser` layer. These models fall into two categories based on
  their workflow:

  - **Standalone On-device Model (CPSSv1)**: This is a TFLite model that uses
    statistical signals about the client's past interactions with permission
    prompts to predict a grant likelihood. It operates entirely on-device
    without needing a server-side call, providing a complete prediction
    workflow.

  - **Stacked On-device AI Models (AIv3, AIv4)**: These models are part
    of a more multi-stage workflow. They run on-device to analyze page content
    and generate a [`PermissionRequestRelevance`][request_relevance] score.
    This score is the only output from these models that is sent to the
    server-side CPSS as an additional feature; no actual page content is sent.

    - **AIv3**: Uses a snapshot of the web page as input.
    - **AIv4**: Uses both a page snapshot and text embeddings (generated by
      `PassageEmbedderDelegate`) as input.
    - **AIv5**: Uses both a page snapshot and text embeddings (generated by
      `PassageEmbedderDelegate`) as input.

The low-level handlers and executors for the CPSSv1, AIv3, and AIv4 models are
implemented in `//components/permissions/prediction_service`.

### Helper Components

- **[`LanguageDetectionObserver`][language_detection_observer]**: Determines the language of the web page, a
  prerequisite for the language-specific passage embedding model, that embeds the
  rendered text as input for AIv4.
- **[`PassageEmbedderDelegate`][passage_embedder_delegate]**: Manages calls to the passage embedding model to
  generate text embeddings from page content.

### Server Communication

- **[`PredictionServiceRequest`][prediction_service_request]**: Encapsulates a request to the server-side
  [`PredictionService`][prediction_service].
- **[`PredictionServiceFactory`][prediction_service_factory]**: A factory for creating the [`PredictionService`][prediction_service]
  instance.

## Prediction Workflows

The prediction service uses a combination of a rule-based check that runs
in parallel with a more sophisticated, multi-stage ML pipeline. The service's
core logic, implemented in the `PermissionsAiUiSelector`, employs a
"model stacking" approach: on-device AI models (AIv3, AIv4, AIv5) run first to
generate a `PermissionRequestRelevance` score. This score is then used as an
input feature for the main server-side model, which makes the final grant
likelihood prediction.

The `PermissionsAiUiSelector::GetPredictionTypeToUse` method
selects the appropriate ML workflow based on user settings (e.g.,
"Make searches and browsing better
(`unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled`)"),
feature flags, and the availability of on-device models.

### Rule-Based Check

This workflow provides a rule-based check for notification permission
requests. It runs in parallel with the ML-based workflows and can trigger a
quiet UI or a warning based on preloaded data and Safe Browsing verdicts.

### ML-Based Predictions

The specific ML-based workflow is chosen by
`GetPredictionTypeToUse` based on a priority order (AIv5 > AIv4 > AIv3 >
On-Device CPSSv1 > Server-Side only). If a higher-priority model is unavailable or
its preconditions are not met, the service falls back to the next one in the
sequence.

#### Standalone On-Device Model

This workflow uses a single on-device model to make a final prediction without
requiring a server-side call.

- **Model**: `kOnDeviceCpssV1Model`
- **Preconditions**:
  - "Make searches and browsing better
    (`unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled`)"
    is not enabled for the client
  - The on-device prediction feature is enabled for the permission type.
  - The client has at least 4 previous permission prompts for the given type.
- **Input**:
  - **Client-specific signals**: Statistical aggregates of the client's past
    permission actions (e.g., grant/deny/dismiss counts), both for the
    specific permission type and for all permission types combined.
  - **Contextual signals**: Request gesture type.
- **Error Handling**: If the model is not available or fails to execute, the
  workflow falls back to the normal UI.
- **Output**: The model predicts a grant likelihood. If it's "VERY_UNLIKELY",
  a quiet UI is shown. Otherwise, the normal UI is used.

#### Stacked Models (On-Device AI + Server-Side)

These workflows use a two-stage process and are **only active if the user has
enabled "Make searches and browsing better
(`unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled`)"**.
First, an on-device AI model generates a `PermissionRequestRelevance` score.
This score is then sent, along with other features, to the server-side CPSS
model for the final grant likelihood prediction.

**Baseline: Server-Side Only**

This is the baseline workflow when no on-device AI models are active.

- **Model**: `kServerSideCpssV3Model`
- **Preconditions**:
  - The server-side prediction feature is enabled
    (`permissions::features::kPermissionPredictionsV2`).
  - No on-device AI models (AIv3, AIv4, AIv5) are enabled.
- **Input**:
  - **Aggregated client-specific signals**: The client's aggregated
    permission action history.
  - **Contextual signals**: Request gesture type.
  - **URL-specific signals**: Requesting origin URL.
- **Error Handling**: If the request to the server fails or returns an empty
  response, the workflow falls back to the normal UI.
- **Output**: The server returns a grant likelihood. If it's "VERY_UNLIKELY",
  a quiet UI is shown. Otherwise, the normal UI is used.

**Stage 1: On-Device AI Models**

These models execute first to generate the relevance score.

- **Model**: `kOnDeviceAiv3AndServerSideModel`

  - **Preconditions**: The AIv3 feature is enabled
    (`permissions::features::kPermissionsAIv3`).
  - **Input**: A snapshot of the web page.
  - **Error Handling**: If snapshot generation or model execution fails, the
    workflow proceeds to the server-side model without the relevance score.

- **Model**: `kOnDeviceAiv4AndServerSideModel`

  - **Preconditions**: The AIv4 feature is enabled
    (`permissions::features::kPermissionsAIv4`).
  - **Input**:
    - A snapshot of the web page.
    - Text embeddings generated from the page's content (requires page
      language to be English).
  - **Error Handling**: If language detection, text embedding, snapshot
    generation, or model execution fails, the workflow proceeds to the
    server-side model without the relevance score.

- **Model**: `kOnDeviceAiv5AndServerSideModel`

  - **Preconditions**: The AIv5 feature is enabled
    (`permissions::features::kPermissionsAIP92`).
  - **Input**:
    - A snapshot of the web page.
    - Text embeddings generated from the page's content (requires page
      language to be English).
  - **Error Handling**: If language detection, text embedding, snapshot
    generation, or model execution fails, the workflow proceeds to the
    server-side model without the relevance score.

**Stage 2: Server-Side Model (with AI Score)**

- **Input**: All signals from the baseline server-side model, plus the
  `PermissionRequestRelevance` score from the on-device AI model that ran in stage 1.
- **Output / Error Handling**: Same as the baseline server-side model.

## Testing

The component has a suite of unit tests and browser tests.

### Unit Tests

Unit tests are located in `prediction_based_permission_ui_selector_unittest.cc`
and `contextual_notification_permission_ui_selector_unittest.cc`. They cover
the logic of the UI selectors in isolation.

To run the PredictionBasedUiSelector unit tests with logging:

```sh
tools/autotest.py --output-directory out/Default chrome/browser/permissions/prediction_service/prediction_based_permission_ui_selector.cc \
--test-launcher-bot-mode --single-process-tests --fast-local-dev \
--enable-logging=stderr --v=0 --vmodule="*/permissions/*"=2,"*/optimization_guide/*"=2
```

### Browser Tests

Browser tests are in `prediction_service_browsertest.cc`. These tests cover the end-to-end functionality, including interactions with the `PermissionRequestManager` and mock model handlers.

To run the browser tests locally:

```sh
tools/autotest.py --output-directory out/Default chrome/browser/permissions/prediction_service/prediction_service_browsertest.cc \
--test-launcher-bot-mode --test-launcher-jobs=1 --fast-local-dev
```

To run the browser tests locally with debugging output one needs to select one single test and use the --single-process-tests flag:

```sh
tools/autotest.py --output-directory out/Default chrome/browser/permissions/prediction_service/prediction_service_browsertest.cc \
--test-launcher-bot-mode --fast-local-dev  --enable-logging=stderr --single-process-tests --v=0 \
--vmodule="*/passage_embeddings/*"=5,"*/permissions/*"=2,"*/optimization_guide/*"=2 --gtest_filter="OneSingleTest"
```

## Relevant Context

- **Core Interfaces:**
  - [`//chrome/browser/permissions/prediction_service/permissions_ai_ui_selector.h`](https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/permissions_ai_ui_selector.h)
  - [`//chrome/browser/permissions/prediction_service/contextual_notification_permission_ui_selector.h`](https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/contextual_notification_permission_ui_selector.h)
- **Model and Handler Providers:**
  - [`//chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h`](https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h)
- **Feature Extraction:**
  - [`//chrome/browser/permissions/prediction_service/language_detection_observer.h`](https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/language_detection_observer.h)
  - [`//chrome/browser/permissions/prediction_service/passage_embedder_delegate.h`](https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/passage_embedder_delegate.h)
- **Components Layer:** The `chrome` layer implementation relies on the core logic in `//components`. See [`//components/permissions/prediction_service/README.md`](https://cs.chromium.org/chromium/src/components/permissions/prediction_service/README.md) for more details.
- **Key Tests:**
  - [`//chrome/browser/permissions/prediction_service/prediction_based_permission_ui_selector_unittest.cc`](https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/prediction_based_permission_ui_selector_unittest.cc)
  - [`//chrome/browser/permissions/prediction_service/contextual_notification_permission_ui_selector_unittest.cc`](https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/contextual_notification_permission_ui_selector_unittest.cc)
  - [`//chrome/browser/permissions/prediction_service/prediction_service_browsertest.cc`](https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/prediction_service_browsertest.cc)
- **Find Call Sites:** The `PermissionRequestManager` invokes `SelectUiToUse` on its configured selectors. See [this Code Search query](https://cs.chromium.org/search/?q=SelectUiToUse+file:components/permissions/permission_request_manager.cc) for the primary invocation site.

[components_readme]: https://cs.chromium.org/chromium/src/components/permissions/prediction_service/README.md
[contextual_ui_selector]: https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/contextual_notification_permission_ui_selector.h
[language_detection_observer]: https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/language_detection_observer.h
[model_handler_provider]: https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h
[passage_embedder_delegate]: https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/passage_embedder_delegate.h
[prediction_based_ui_selector]: https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/permissions_ai_ui_selector.h
[prediction_service]: https://cs.chromium.org/chromium/src/components/permissions/prediction_service/prediction_service.h
[prediction_service_factory]: https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/prediction_service_factory.h
[prediction_service_request]: https://cs.chromium.org/chromium/src/chrome/browser/permissions/prediction_service/prediction_service_request.h
[request_manager]: https://cs.chromium.org/chromium/src/components/permissions/permission_request_manager.h
[request_relevance]: https://cs.chromium.org/chromium/src/components/permissions/permission_request_enums.h?q=PermissionRequestRelevance
[ui_selector]: https://cs.chromium.org/chromium/src/components/permissions/prediction_service/permission_ui_selector.h
