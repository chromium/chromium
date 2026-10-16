---
name: general/accurately-describe-reference-returns-in-comments
rubric: readability
severity: nit
---

**Rule.** When documenting functions or accessors that return a reference (const
T&, T&), pointer, or non-owning view (std::string_view, base::span), describe
the returned object directly without claiming to return 'a copy of' it. Reserve
copy and construction phrasing exclusively for functions that return by value or
create new independent instances.

**Why.** Describing a function that returns a reference or view as returning 'a
copy' is misleading: it causes callers to assume the call incurs copy
construction or heap allocation overhead, or that they receive an independent,
mutable instance whose lifetime is not tied to an underlying object.

## Examples

Bad:

```cpp
// Returns a copy of the command line as it existed immediately after
// initialization in the browser process.
const base::CommandLine& GetInitialBrowserCommandLine();

// Returns a copy of the user's display name.
std::string_view GetDisplayName() const;
```

Good:

```cpp
// Returns the command line as it existed immediately after
// initialization in the browser process.
const base::CommandLine& GetInitialBrowserCommandLine();

// Returns the user's display name.
std::string_view GetDisplayName() const;
```
