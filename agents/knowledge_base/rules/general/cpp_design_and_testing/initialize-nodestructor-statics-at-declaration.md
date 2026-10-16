---
name: general/initialize-nodestructor-statics-at-declaration
rubric: design
severity: should_fix
---

**Rule.** When defining a static snapshot, singleton, or cached object with
`base::NoDestructor<T>`, initialize the function-local static variable directly
with its initial value in its declaration. Do not create separate helper
functions returning mutable references, global initialization-tracking booleans,
or manual two-phase assignment routines to populate the static instance.

**Why.** C++ function-local static variables are thread-safe and initialized
upon first execution of their declaration. Introducing separate mutable storage
accessors, tracking booleans, and manual assignment routines adds unnecessary
indirection, creates mutable global state, and risks uninitialized reads
compared to direct lazy initialization.

## Examples

Bad:

```cpp
namespace {
bool g_initial_command_line_captured = false;
base::CommandLine& GetInitialCommandLineStorage() {
  static base::NoDestructor<base::CommandLine> initial_command_line(
      base::CommandLine::NO_PROGRAM);
  return *initial_command_line;
}
}  // namespace

const base::CommandLine& GetInitialBrowserCommandLine() {
  CHECK(g_initial_command_line_captured);
  return GetInitialCommandLineStorage();
}

// In caller/initialization:
GetInitialCommandLineStorage() = *base::CommandLine::ForCurrentProcess();
g_initial_command_line_captured = true;
```

Good:

```cpp
const base::CommandLine& GetInitialBrowserCommandLine() {
  CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType));
  static base::NoDestructor<base::CommandLine> initial_command_line(
      *base::CommandLine::ForCurrentProcess());
  return *initial_command_line;
}

// In caller/initialization:
GetInitialBrowserCommandLine();
```
