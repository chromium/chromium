---
name: general/structure-component-landing-pages-progressively
rubric: readability
severity: nit
---

**Rule.** Structure subsystem and component landing pages (`README.md`)
progressively for newcomers: start with a plain-language explanation of what the
subsystem does and why/where to use it, provide 'Getting started' guidance for
common use cases, delegate advanced topics to separate sections or sub-guides,
and list support channels or bug components rather than dumping technical jargon
or disconnected ecosystem tools upfront.

**Why.** Subsystem landing pages are primary entry points for developers and
newcomers. Leading with disconnected ecosystem tools or unexplained technical
jargon creates high cognitive overhead; progressive disclosure from basic
purpose to common tasks and advanced reference guides readers smoothly into
practical usage.

## Examples

Bad:

```cpp
# Chromium Fuzzing

Regardless of the framework you choose, your fuzzer will be part of a larger ecosystem:
* [ClusterFuzz]: Our automated infrastructure...
* [libFuzzer]: An in-process, coverage-guided fuzzing engine...
* [AFL]: Another popular fuzzing engine...
* **Sanitizers**: Powerful tools like AddressSanitizer...
* [Blackbox fuzzing]: A technique for large, slow, or non-deterministic targets...
```

Good:

```cpp
# Fuzzing in Chromium

Fuzzing is an automated software testing technique that feeds invalid, unexpected, or random data into a program to find bugs and security vulnerabilities.

**Why fuzz?** Fuzzing catches thousands of memory-safety and stability bugs before release.
**Where to fuzz?** Any parser, decoder, IPC handler, or code processing untrusted inputs.

## Getting started
Choose the fuzzing framework suited for your target:
* [FuzzTest](fuzztest.md) - For C++ unit testing.
* [libFuzzer](libfuzzer.md) - For coverage-guided fuzzing.

## Advanced topics
* [ClusterFuzz integration](clusterfuzz.md)
* [Sanitizer configurations](sanitizers.md)

## Getting help
File issues under `Tools > Fuzzing` on `issues.chromium.org` or email `fuzzing@chromium.org`.
```
