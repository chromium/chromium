---
name: general/prefer-channel-unknown-over-channel-default
rubric: readability
severity: should_fix
---

**Rule.** In C++ code referencing `version_info::Channel`, use
`version_info::Channel::UNKNOWN` rather than `version_info::Channel::DEFAULT`.
Treat `Channel::DEFAULT` solely as a compatibility alias reserved for build
files.

**Why.** In `base/version_info/channel.h`, `Channel::DEFAULT` is an alias for
`Channel::UNKNOWN` kept for GN/build configuration compatibility, whereas
Chromium C++ source code standardizes on `Channel::UNKNOWN`.

## Examples

Bad:

```cpp
if (channel == version_info::Channel::DEFAULT) {
  DoFallback();
}
```

Good:

```cpp
if (channel == version_info::Channel::UNKNOWN) {
  DoFallback();
}
```
