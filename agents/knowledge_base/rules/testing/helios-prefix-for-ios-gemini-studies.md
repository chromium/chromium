---
name: testing/helios-prefix-for-ios-gemini-studies
rubric: readability
severity: nit
---

**Rule.** In `fieldtrial_testing_config.json`, use the `Helios` prefix (e.g.,
`Helios<FeatureName>`) for iOS Gemini Finch study names rather than `Gemini` or
`IOSGemini`. Omit redundant platform prefixes like `IOS` from the study name
since platform targeting is already specified under `platforms`.

**Why.** Using the consistent `Helios` prefix groups all iOS Gemini field trial
studies alphabetically in `fieldtrial_testing_config.json` and Finch management
tools, making them easier to discover and maintain while avoiding redundant
platform prefixes.

## Examples

Bad:

```cpp
"IOSGeminiFloatyAllPages": [
    {
        "platforms": [
            "ios"
        ],
        "experiments": [
            {
                "name": "Enabled",
                "enable_features": [
                    "GeminiFloatyAllPages"
                ]
            }
        ]
    }
]
```

Good:

```cpp
"HeliosFloatyAllPages": [
    {
        "platforms": [
            "ios"
        ],
        "experiments": [
            {
                "name": "Enabled",
                "enable_features": [
                    "GeminiFloatyAllPages"
                ]
            }
        ]
    }
]
```
