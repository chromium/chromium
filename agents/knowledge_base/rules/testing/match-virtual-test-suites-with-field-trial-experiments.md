---
name: testing/match-virtual-test-suites-with-field-trial-experiments
rubric: testing
severity: should_fix
---

**Rule.** When adding or modifying field trial experiments for web-exposed or
Blink features in `testing/variations/fieldtrial_testing_config.json`, verify
that corresponding Blink virtual test suites in
`third_party/blink/web_tests/VirtualTestSuites` are added or updated with
matching `--enable-features` arguments. Ensure test suites run against the
experimental flag configuration so web tests can detect behavioral changes and
regressions.

**Why.** Waterfall and try bots execute Blink web tests against virtual test
suites to validate non-default flag configurations. Omitting or mismatching
virtual test suites for active field trial experiments prevents web tests from
exercising experimental behavior, leaving regressions undetected prior to
launch.

## Examples

Bad:

```cpp
// In testing/variations/fieldtrial_testing_config.json:
"SymphoniaAdditionalCodecs": [
  {
    "platforms": ["linux", "mac", "windows"],
    "experiments": [
      {
        "name": "Enabled",
        "enable_features": ["SymphoniaPcmDecoding", "SymphoniaVorbisDecoding"]
      }
    ]
  }
]
// Anti-pattern: No matching virtual test suite configured in third_party/blink/web_tests/VirtualTestSuites.
```

Good:

```cpp
// In third_party/blink/web_tests/VirtualTestSuites:
{
  "prefix": "symphonia-additional-codecs",
  "platforms": ["Linux", "Mac", "Win"],
  "bases": ["webaudio/codec-tests"],
  "args": ["--enable-features=SymphoniaPcmDecoding,SymphoniaVorbisDecoding"],
  "expires": "Jan 1, 2027"
}
```
