---
name: loadline
description: Running and analyzing LoadLine 1 and 2 benchmarks on Android using Crossbench. Use when you need to measure page loading performance, evaluate performance-related changes in Chrome, or collect Perfetto traces with realistic Chrome workload.
---

# LoadLine Benchmark

This skill guides the usage of LoadLine benchmarks to measure browser page
loading performance by simulating real-world user journeys (loading a web page).

## Quick Start

Read `third_party/crossbench/config/benchmark/loadline2/README.md` for basic
info on the LoadLine benchmark.

The entry point for all LoadLine benchmarks is the Crossbench script:
`third_party/crossbench/cb.py`

### Common Command Pattern

```bash
third_party/crossbench/cb.py <variant> --browser=<browser> --stories=<stories> --repetitions=<N>
```

## Benchmark Variants

| Variant | Description |
| :--- | :--- |
| `loadline2-phone` | The latest LoadLine 2 benchmark optimized for mobile phones. |
| `loadline2-tablet` | LoadLine 2 benchmark optimized for tablets. |
| `loadline-phone` | Legacy LoadLine 1 benchmark for phones. |
| `loadline-tablet` | Legacy LoadLine 1 benchmark for tablets. |
| `*-debug` | Appends more tracing categories for easier debugging. |

## Target Browsers (`--browser`)

- **Android (Chrome):** Use `--browser=adb:chrome` (if one device) or
  `--browser=$SERIAL:chrome`.
- **Local Build:** Use the path to the output directory, e.g.,
  `--browser=out/Release/chrome`.
- **System Chrome:** Use `--browser=chrome-stable`, `chrome-canary`, etc.

## Stories (`--stories`)

Default stories for LoadLine 2:

- `amazon_product`
- `cnn_article`
- `wikipedia_article`
- `globo_homepage`
- `google_search_result`

Skip `--stories` flag to run everything or provide a comma-separated list for
specific stories.

## Analyzing Results

LoadLine outputs two kinds of numbers: scores and breakdown.

Scores are in runs-per-minute, so higher is better. The main metric is called
TOTAL\_SCORE, this is the most stable and representative number.

Breakdown values are in milliseconds, so lower is better. Can be useful for
analyzing which loading stage was affected in particular.

### Statistical Significance

- **Noise:** Benchmark results are inherently noisy. A single repetition should
  only be used for smoke-testing.
- **Reliability:** To confidently determine changes of **1% or higher**, run the
  benchmark with at least **50 repetitions**.
- **Interpretation:** Changes below 1% are typically not considered
  statistically significant, even with 50 repetitions.

### Identifying Tested Version

You can find the exact version of Chrome being tested in the benchmark logs by
looking for the `🏷️ STARTING BROWSER Version:` line:

```
🏷️  STARTING BROWSER Version: 138.0.7204.168 stable
```

## Common Workflows

### Full Basic Run

Run the full benchmark on a connected Android device:

```bash
third_party/crossbench/cb.py loadline2-phone --browser=adb:chrome
```

### Test a feature flag

See if a feature flag affects page loading performance:

```bash
third_party/crossbench/cb.py loadline2-phone --browser-config=feature_flag.hjson
```

where `feature_flag.hjson` looks like this:

```
{
  flags: {
    "experiment": {
      "enabled": "--enable-features=YourFeature",
      "disabled": "--disable-features=YourFeature",
    },
  },
  browsers: {
    "chrome": {
      browser: "chrome",
      driver: "adb",
      flags: [ "experiment" ],
    },
  },
}
```

### Compare past versions of Chrome

Check if `clank/bin/install_chrome.py` exists. If not (chromium-only checkouts),
see the next section for building Chromium locally.

Use `clank/bin/install_chrome.py` to install past versions on the connected
device. E.g.

```
clank/bin/install_chrome.py --channel dev --milestone 146 --signed
```

will install M146 with "Dev" channel branding. You can use multiple brandings to
compare versions on the same device. E.g. install Canary:

```
clank/bin/install_chrome.py --channel canary --milestone 147 --signed
```

And then run LoadLine with two browsers:

```
third_party/crossbench/cb.py loadline2-phone --browser=adb:chrome-dev --browser=adb:chrome-canary
```

This will compare M146 and M147 on the same device.

### Custom-built Chrome

1. Build Chrome for Android with `android_channel="canary"` gn arg.
1. Uninstall existing Chrome Canary from the device:
   `adb shell pm uninstall com.chrome.canary` (this command fails if it's not
   installed; this is fine and can be ignored)
1. Install the custom built Chrome: `adb install out/$OUT_DIR/apks/Chrome.apk`
1. Run LoadLine on chrome-canary:
   `third_party/crossbench/cb.py loadline2-phone --browser=adb:chrome-canary`

### Short run

Before running a full run on a custom-built Chrome, first try with a single
repetition

```bash
third_party/crossbench/cb.py loadline2-phone --browser=adb:chrome-canary --repetitions=1
```

Don't trust the numbers, they are going to be very noisy. Do it just to verify
that the benchmark runs without errors.
