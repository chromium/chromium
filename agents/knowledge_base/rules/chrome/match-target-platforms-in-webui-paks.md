---
name: chrome/match-target-platforms-in-webui-paks
rubric: design
severity: should_fix
---

**Rule.** When packaging WebUI resources in `chrome_extra_paks`
(`chrome/chrome_paks.gni`), add desktop `.pak` files to `sources` under
`if (!is_android)` by default and match any custom platform conditions to the
`WebUIConfig` registration in `chrome/browser/ui/webui/chrome_web_ui_configs.cc`
(including `is_chromeos` or `is_desktop_android`). Do not add individual WebUI
resource targets (`//chrome/browser/resources/<feature>:resources`) directly to
`deps` in `chrome_extra_paks`; add them to `group("resources")` in
`chrome/browser/resources/BUILD.gn` instead, which `chrome_extra_paks` already
depends on.

**Why.** Android has strict binary and APK size constraints and separate
resource packaging; bundling desktop resources unconditionally or with
mismatched platform guards causes runtime missing-resource errors or dead-code
binary bloat. Centralizing WebUI `:resources` target dependencies in
`chrome/browser/resources/BUILD.gn` prevents duplicating build graph
dependencies already provided by the aggregate `resources` group.

## Examples

Bad:

```cpp
# chrome/chrome_paks.gni
template("chrome_extra_paks") {
  # Bad: unconditional pak, mismatched platform guards, and direct target deps
  sources += [ "$root_gen_dir/chrome/sample_feature_resources.pak" ]
  if (is_win || is_mac || is_linux) {  # Missing is_chromeos or WebUIConfig guard
    sources += [ "$root_gen_dir/chrome/discards_resources.pak" ]
  }
  deps += [ "//chrome/browser/resources/glic/experimental_opt_in:resources" ]
}
```

Good:

```cpp
# chrome/chrome_paks.gni
template("chrome_extra_paks") {
  if (!is_android) {
    sources += [ "$root_gen_dir/chrome/sample_feature_resources.pak" ]
  }
  if (is_win || is_mac || is_linux || is_chromeos || is_desktop_android) {
    sources += [ "$root_gen_dir/chrome/discards_resources.pak" ]
  }
  # Note: chrome_extra_paks already depends on "//chrome/browser/resources"
}

# chrome/browser/resources/BUILD.gn
group("resources") {
  public_deps += [ "glic/experimental_opt_in:resources" ]
}
```
