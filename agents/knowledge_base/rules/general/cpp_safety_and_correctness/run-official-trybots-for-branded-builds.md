---
name: general/run-official-trybots-for-branded-builds
rubric: correctness
severity: should_fix
---

**Rule.** When modifying build files, dependencies, assets, or code guarded by
`is_chrome_branded` (or `GOOGLE_CHROME_BRANDING`, `_google_chrome`,
`BuildConfig.IS_CHROME_BRANDED`) or referencing internal repositories,
explicitly select and run official build trybots (such as `win-official`,
`mac-official`, or `linux-chromeos-chrome`) to verify branded targets before
landing.

**Why.** Default CQ trybots only compile and test open-source Chromium
configurations (`is_chrome_branded = false`). Changes conditioned on branded
builds or internal repositories are never compiled in standard CQ runs, allowing
syntax, packaging, and dependency errors in official builds to pass undetected.

## Examples

Bad:

```cpp
# Modifying branded-only dependencies or source sets in BUILD.gn:
if (is_chrome_branded) {
  deps += [ "//chrome/browser/internal:branded_features" ]
}
# Relying solely on default CQ trybots before landing.
```

Good:

```cpp
# Modifying branded-only dependencies or source sets in BUILD.gn:
if (is_chrome_branded) {
  deps += [ "//chrome/browser/internal:branded_features" ]
}
# Explicitly selecting and running official trybots (e.g. mac-official, win-official, linux-chromeos-chrome) to verify branded builds before landing.
```
