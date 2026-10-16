---
name: general/guard-platform-strings-and-webui-resources-at-build-time
rubric: safety
severity: should_fix
---

**Rule.** Guard platform-specific UI strings in .grd/.grdp files, WebUI
HTML/TS/CSS resources, and C++ content::WebUIDataSource bindings with build-time
platform conditions (such as <if expr="is_win"> and #if BUILDFLAG(IS_WIN)). Do
not include platform-specific strings or assets unconditionally or pass dummy
fallback values at runtime on unsupported platforms.

**Why.** Unconditionally declaring platform-specific strings in .grd files
causes them to be translated across dozens of locales and packaged into .pak
files on all platforms, causing unnecessary binary and resource size bloat.
Similarly, omitting platform guards in WebUI assets bundles dead frontend code
and runtime fallback bindings into unsupported builds.

## Examples

Bad:

```cpp
<!-- In .grd: -->
<message name="IDS_SETTINGS_PLATFORM_FEATURE_LABEL" desc="...">
  Platform feature description
</message>

// In C++ WebUIDataSource setup:
html_source->AddBoolean(
    "isPlatformFeatureEnabled",
#if BUILDFLAG(IS_WIN)
    features::IsPlatformFeatureEnabled()
#else
    false
#endif
);
```

Good:

```cpp
<!-- In .grd: -->
<if expr="is_win">
  <message name="IDS_SETTINGS_PLATFORM_FEATURE_LABEL" desc="...">
    Platform feature description
  </message>
</if>

// In C++ WebUIDataSource setup:
#if BUILDFLAG(IS_WIN)
html_source->AddBoolean("isPlatformFeatureEnabled",
                        features::IsPlatformFeatureEnabled());
#endif
```
