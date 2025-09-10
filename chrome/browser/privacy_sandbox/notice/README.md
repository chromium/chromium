# Privacy Sandbox Notice Framework

This framework provides a centralized system for managing user-facing Privacy Sandbox notices in Chrome. It offers a scalable and maintainable approach to presenting notices.

## High-Level Approach

The Notice Framework shifts notice management from an imperative to a declarative model. Instead of implementing step-by-step logic to check eligibility and manage state, developers define a notice and its rules in the `NoticeCatalog`. The framework then handles prioritization, conflict resolution, state tracking, and metrics.

The core differences are:

| | **Traditional Approach** | **Framework Approach** |
| :--- | :--- | :--- |
| **Decision Logic** | Imperative & Centralized: "Check A, then B, then C..." | Declarative & Orchestrated: "Here are the available notices and their rules; pick the best one." |
| **State Management**| Direct manipulation of individual preferences. | Abstracted via `NoticeStorage`, providing a clean, structured history. |
| **Extensibility** | Difficult; requires modifying complex, shared logic. | Simple; add a new, self-contained definition to the `NoticeCatalog`. |
| **Developer Focus**| Implementing complex eligibility trees and managing state. | Defining the notice, its target APIs, and its desired behavior. |

### Key Components

The framework provides the following functionality:

#### 1. Orchestration
The framework's central `NoticeService` handles notice scheduling and prioritization.
*   **Conflict Resolution**: If multiple notices are eligible to be shown, the orchestrator uses rules and priorities defined in the `NoticeCatalog` to select the most appropriate one.
*   **Notice Grouping and Prioritization**: The framework can group related notices that belong to a single user flow (e.g., a multi-step consent). Each notice in a group is assigned a priority, ensuring that the orchestrator displays them in the correct sequence. This mechanism also allows for delivering tailored versions of a notice for different UI surfaces (e.g., desktop vs. mobile).
*   **Dependency Management**: The framework supports defining API prerequisites for notices. This is used to manage notice flows where a user must have acknowledged a notice for one API before becoming eligible for a notice about another.

#### 2. State Management and Metrics
The `NoticeStorage` component handles all notice history and data collection.
*   **Persistent History**: `NoticeStorage` tracks the event history for each notice (e.g., when it was shown, what action was taken) for each user profile. This removes the need for direct `PrefService` management.
*   **Standardized Histograms**: The framework automatically records a standard set of UMA metrics for every notice, including user interaction counts and timing durations. This provides consistent data without requiring custom histogram code.

#### 3. API and Notice Versioning
The orchestration logic is designed to manage notice requirements as APIs evolve. By defining separate notices for different API versions and using dependency rules in the `NoticeCatalog`, the orchestrator can distinguish between new and existing users. This allows it to serve a full notice to new users or a smaller, delta notice to users who have already acknowledged a previous version.

#### 4. Custom Eligibility Logic
For complex eligibility scenarios not covered by the framework's standard rules, you can attach a custom C++ callback to a `NoticeApi` definition in the `NoticeCatalog`. The orchestrator executes this callback during its eligibility evaluation, allowing for fine-grained, feature-specific control over when a notice can be shown.

## Documentation

*   **[Getting Started Guide](docs/getting_started.md)**: A step-by-step guide to adding a new notice. This is the best place to start if you need to implement a notice.
*   **[Architecture Deep Dive](docs/architecture.md)**: For a detailed understanding of the framework's internal components (Orchestrator, Catalog, etc.) and how they interact.