---
name: general/separate-api-removal-from-caller-migrations
rubric: change_scope
severity: should_fix
---

**Rule.** When making API changes or migrating callers across the codebase—such
as deleting obsolete methods, adding \[[nodiscard]\] attributes, changing global
defaults, adding parameters requiring call-site plumbing (e.g., base::Location),
or replacing high-risk OS/platform API invocations (e.g., ShellExecute,
LaunchProcess)—do not bundle widespread call-site migrations with core
declaration changes, nor convert all callers across multiple subsystems in a
single monolithic CL. For breaking changes or enforcement attributes, land
preparatory call-site migrations in preceding CLs before changing the API
declaration; for additive parameter plumbing, provide default arguments (or
non-breaking overloads) so the core API lands first before updating callers; and
for widespread platform or subsystem migrations, partition caller updates into
incremental CLs grouped by entry point or subsystem.

**Why.** Bundling widespread caller plumbing with core API modifications or
converting multi-subsystem call sites in a single monolithic CL crosses multiple
subsystem OWNERS boundaries, inflates review size beyond 500 LoC, and magnifies
revert risk. Splitting core API changes from caller migrations and partitioning
widespread call-site updates by entry point isolates functional changes, enables
smaller focused reviews, simplifies regression bisection, and prevents full
reverts if caller updates fail on specific platforms.

## Examples

Bad:

```cpp
// Variant 1: Bundling API deletion with widespread caller migrations in one CL:
-BASE_EXPORT int RandInt(int min, int max);
 BASE_EXPORT int RandIntInclusive(int min, int max);

// Variant 2: Bundling [[nodiscard]] with call-site fixes across tests in one CL:
 std::ignore = base::BindOnce([](raw_ptr<int> ptr) {}, ptr);
 template <typename Functor, typename... Args>
+[[nodiscard]] inline auto BindOnce(Functor&& functor, Args&&... args);

// Variant 3: Bundling global default change with call-site migrations in one CL:
 template <class ObserverType,
-          ObserverListReentrancyPolicy reentrancy = ObserverListReentrancyPolicy::kAllowReentrancy>
+          ObserverListReentrancyPolicy reentrancy = ObserverListReentrancyPolicy::kDisallowReentrancy>
 class ObserverList { ... };

// Variant 4: Bundling multi-subsystem OS/platform API migrations across chrome/, base/, and ui/:
- ShellExecute(nullptr, L"open", path.value().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
+ ui::win::OpenFileViaShell(path);
- ShellExecute(nullptr, nullptr, url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
+ platform_util::OpenExternal(profile, url);
```

Good:

```cpp
// Variant 1: Split API deletion (CL 1: migrate callers; CL 2: delete RandInt declaration):
-BASE_EXPORT int RandInt(int min, int max);

// Variant 2: Split [[nodiscard]] (CL 1: fix call sites; CL 2: add [[nodiscard]] attribute):
template <typename Functor, typename... Args>
[[nodiscard]] inline auto BindOnce(Functor&& functor, Args&&... args);

// Variant 3: Split additive plumbing (CL 1: default arg; CL 2+: update callers):
void ExecuteJavaScript(WKWebView* web_view, NSString* script,
                       completion_handler,
                       const base::Location& location = FROM_HERE);

// Variant 4: Split multi-subsystem caller migrations into focused CLs by entry point:
// CL 1: Migrate file/folder explorer entry points in ui/base to ui::win::OpenFileViaShell.
// CL 2: Follow-up CL migrating browser external URL handlers in chrome/browser.
```
