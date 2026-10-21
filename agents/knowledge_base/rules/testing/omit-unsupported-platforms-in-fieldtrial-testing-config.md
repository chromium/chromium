---
name: testing/omit-unsupported-platforms-in-fieldtrial-testing-config
rubric: correctness
severity: should_fix
---

**Rule.** In `testing/variations/fieldtrial_testing_config.json`, verify that
the `platforms` array accurately matches the feature's targeted operating
systems rather than copy-pasting desktop platform lists (e.g. substituting
`chromeos` for `android`). Target all applicable platforms for shared
cross-platform features, including `"android_webview"` alongside `"android"` for
shared web-platform and Blink (`third_party/blink`) features unless WebView is
intentionally excluded. Conversely, omit platforms where the feature is
unsupported (such as `"ios"` for Blink and `content/` features).

**Why.** The `platforms` array controls which waterfall test bots and developer
builds execute field trial experiments. Inadvertent platform mismatches or
omitted mobile platforms (like `android` or `android_webview`) leave targeted
platforms unexercised during CI testing, while listing unsupported platforms
(such as `ios` for WebKit-based ports) clutters configuration metadata and
wastes bot resources.

## Examples

Bad:

```cpp
// Missing android_webview for Blink feature or wrong platforms from copy-paste:
"ContentBindingVersioningEnabled": [
    {
        "platforms": [
            "chromeos",
            "linux",
            "mac",
            "windows"
        ],
        "experiments": [...]
    }
],
"BlinkFeatureStudy": [
    {
        "platforms": ["android", "ios", "linux", "mac", "windows"],
        "experiments": [...]
    }
]
```

Good:

```cpp
// Correct targeted platforms, including android_webview and omitting ios:
"ContentBindingVersioningEnabled": [
    {
        "platforms": [
            "android",
            "linux",
            "mac",
            "windows"
        ],
        "experiments": [...]
    }
],
"BlinkFeatureStudy": [
    {
        "platforms": ["android", "android_webview", "linux", "mac", "windows"],
        "experiments": [...]
    }
]
```
