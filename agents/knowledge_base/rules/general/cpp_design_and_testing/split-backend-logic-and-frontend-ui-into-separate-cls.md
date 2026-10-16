---
name: general/split-backend-logic-and-frontend-ui-into-separate-cls
rubric: change_scope
severity: should_fix
---

**Rule.** When introducing features, refactorings, or migrations that span
multiple architectural layers (such as foundational //base utilities vs.
higher-level downstream call sites) or distinct subsystems (such as backend
services, preferences, Mojo IPC interfaces, and frontend WebUI/Views), split the
work into separate, focused CLs along architectural boundaries. Land
foundational infrastructure, core library adjustments, backend validation logic
with unit tests, resource string definitions, and Mojo interfaces in preceding
CLs tagged with the same tracking bug before integrating or migrating downstream
call sites and UI controls in follow-up CLs.

**Why.** Combining foundational platform utilities, backend logic, IPC
interfaces, and widespread UI or call-site modifications into a single
monolithic CL crosses multiple subsystem ownership boundaries, inflates review
size beyond 500 LoC, and lengthens review latency. Splitting work along
architectural boundaries enables independent review and parallel approval by
domain OWNERS, isolates CI flake and revert risks, simplifies regression
bisection, and ensures foundational improvements remain intact if higher-level
integrations require reversion.

## Examples

Bad:

```cpp
// In a single monolithic CL spanning multiple architectural layers:
// - Update base/ supplier and promise thread-checking behavior
// - Register new preference in BrowserProcessImpl and update backend validation logic
// - Add localized strings in .grdp resource files and adjust WebUI layout
// - Define new mojom interface and wire settingsPrivate / IPC to frontend
// - Simultaneously refactor ToolbarManager accessors across dozens of UI files
```

Good:

```cpp
// Split into focused, incremental CLs linked by the same tracking bug:
// CL 1 (foundational/base): Update supplier and promise thread-checking behavior in base/.
// CL 2 (backend): Update backend validation logic, register preference, and add unit tests.
// CL 3 (strings & IPC): Add localized strings and define mojom interface/handlers.
// CL 4 (frontend/call sites): Refactor ToolbarManager UI accessors and add WebUI components.
```
