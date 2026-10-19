---
name: crossbench-runner
description: >-
  Assists developers in configuring, executing, and analyzing Crossbench
  benchmarks (native commands and web-tests) in Chromium.
trigger: >-
  Load this when the user asks how to run Crossbench, browser_startup,
  web-tests, or where to find trace/CSV performance results.
---

# Crossbench Benchmark Runner

Use this skill when a developer asks for help running a Crossbench benchmark or
finding its results.

### 1. Determining the Runner Type

Crossbench tests in Chromium use one of two runners: **Native Benchmarks**
(`cb.py`) or **Web-Test CUJs** (`run.py`). Use the inventory and rules below to
identify the runner immediately without searching the repository.

#### A. Known Test Inventory

- **Native Benchmarks (`third_party/crossbench/cb.py`)**: Standard synthetic
  press suites, microbenchmarks, and core page-load/startup harnesses:
  - *Press & Synthetic*: `speedometer` (`speedometer_3.1`, `speedometer_3.0`,
    `speedometer_2.1`, `speedometer_main`), `jetstream` (`jetstream_3.0`,
    `jetstream_2.2`, `jetstream_main`), `motionmark` (`motionmark_1.4`,
    `motionmark_1.3`), `webxprt5`
  - *Startup & Page Load*: `browser-startup`, `loadline2-phone`,
    `loadline2-tablet`, `loadline-phone`, `loadline-tablet`, `loading`
  - *Resource, AI & Other*: `memory`, `powerline`, `web-power`, `blink-ai`,
    `webai`, `embedder`, `devtools-frontend`, `manual`
- **Web-Test CUJs**
  (`third_party/crossbench-web-tests/cuj/crossbench/runner/run.py`): Multi-step
  Critical User Journeys (CUJs) and interactive app/site workflows driven by
  `.page-config.hjson` action blocks:
  - *Google Workspace*: `docs`, `docs-tab-switches`, `sheets`, `slides`,
    `gmail`, `classroom`, `drive-cleanup`
  - *Conferencing & RTC*: `meet`, `meet-note`, `heavy-meet-note`,
    `meet-share-note`, `local-conference`, `videocall`, `rtc-peer-connection`
  - *Media & Social*: `youtube`, `youtube-watch-browse`, `tiktok`, `video-drm`,
    `video-drm-seek`, `video-seek`, `video-playback-*`
  - *Browsing & Stress*: `browsing`, `browsing-multitab`,
    `browsing-multiwindow`, `simultaneous-load`, `open-close`, `tab-stress`,
    `cpu-stress`, `memory-pressure`, `gpu-swap`, `click-input-latency`,
    `typing-input-latency`, `page-click`, `page-scroll`, `internet-speed-test`

#### B. Disambiguation & Naming Rules

- **Overlapping Names (Avoid Naive Repo Search)**: Several native benchmarks
  (`browser-startup`, `speedometer_*`, `jetstream_*`, `motionmark_*`,
  `loadline*`, `memory`, `web-power`, `webxprt5`) also have wrapper directories
  under `third_party/crossbench-web-tests/`. **Always default to Native
  (`cb.py`)** for these tests unless the user explicitly mentions `web-tests`,
  `CUJ`, `run.py`, `--variants`, or `--secrets`.
- **Name Normalization**: Users often reference Chromium bot config names or
  underscores (e.g., `browser_startup` or `browser_startup.crossbench` from
  `bot_platforms.py`). Map these to the canonical Crossbench CLI name (e.g.,
  `browser-startup` for `cb.py`).
- **CLI Discovery (For Unknown Tests or Variants)**: Instead of grepping the
  repo, query the runners directly to list valid names, stories, or variants:
  - List all Native Benchmarks and aliases:
    `vpython3 third_party/crossbench/cb.py describe benchmarks`
  - Inspect stories and options for a specific Native Benchmark:
    `vpython3 third_party/crossbench/cb.py describe benchmark <name>`
  - List all Web-Test CUJs and their available `--variants`:
    ```bash
    vpython3 third_party/crossbench-web-tests/cuj/crossbench/runner/run.py \
      --list
    ```

#### C. Runner Commands

**Native Benchmarks (e.g., `browser-startup`, `speedometer_3.1`)**

- **Path**: `third_party/crossbench/cb.py`
- **Command**:
  ```bash
  vpython3 third_party/crossbench/cb.py <benchmark_name> \
    --browser=chrome \
    --repeat=1
  ```

**Web-Tests (e.g., `heavy-meet-note`, `docs`)**

- **Path**: `third_party/crossbench-web-tests/cuj/crossbench/runner/run.py`
- **Command**:
  ```bash
  cd third_party/crossbench-web-tests/cuj/crossbench/runner/
  vpython3 run.py --platform local --tests <test_name> \
    --variants <variant_name>
  ```

### 2. Troubleshooting Local Execution (CloudTop / Linux Desktop)

If the developer encounters crashes running locally, check for these common
environmental limitations:

- **No Display / Headless Crash**: If the browser exits immediately
  (`Could not start WebDriver: session not created: Chrome instance exited`),
  they are likely on a headless VM or missing display environment variables.
  Tell them to:
  - Pass `--headless` (or `--browser=chrome-headless`), prepend `xvfb-run` to
    their command, or
  - When running headed on CloudTop/CRD with an active session, export
    `WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/$(id -u)` (or `DISPLAY`
    for X11).
- **Unsupported `downloads` Probe**: The `downloads` probe is only supported on
  Android and ChromeOS. If running on local Linux/Mac, it will throw
  `NotImplementedError: Probe(DownloadsProbe): Unsupported browser`. Advise them
  to pass `--no-probe=downloads` on the CLI or use a custom `probe-config.hjson`
  that omits the `downloads` probe (or override the template inheritance).
- **Hanging `screenshot` Probe**: If the test hangs indefinitely on a
  VM/CloudTop, the `screenshot` probe is likely waiting for `gnome-screenshot`
  (which requires a physical GNOME desktop). Advise them to pass
  `--no-probe=screenshot` or remove the `screenshot` probe from their config.

### 3. Analyzing the Results

When the developer asks where to find the data after a successful run, guide
them to the `third_party/crossbench/results/` directory generated at the end of
the execution.

**Aggregated Results (High Level)**

- Located at `third_party/crossbench/results/latest/cb.results.json`.
- This file contains merged statistical summaries (min, max, average).
- *Warning*: The generic `trace_processor` probe strips story metadata
  (`cb_story`) during JSON aggregation. Do not rely on this file if you need
  metrics separated by individual stories (e.g., `blank` vs. `newtab`).

**Unmerged Metrics (By Story)**

- To see exact timings separated by story, look at the raw Trace Processor CSVs
  (note that there is no `output/` intermediate directory).
- **Path**:
  `third_party/crossbench/results/latest/trace_processor/<metric_name>.csv`.
- This file retains the `cb_story` and `cb_run` columns, allowing developers to
  see exact latencies for each scenario and repetition.

**Perfetto Traces (For UI Inspection)**

- The raw `.pb.gz` traces are saved per-browser, per-story, per-repetition, and
  per-cache-temperature (`0_default`).
- **Path**:
  `third_party/crossbench/results/latest/<browser_dir>/stories/<story_name>/<repetition>/0_default/perfetto.trace.pb.gz`
  (convenience symlinks `first_run/` and `last_run/` are also created alongside
  the `<repetition>` directories, e.g.,
  `.../stories/<story_name>/first_run/0_default/perfetto.trace.pb.gz`).
- Advise the developer to download this file and open it in
  `https://ui.perfetto.dev/` to debug trace events manually.

### 4. Advanced Configuration Tips

- **Multi-Arm Comparisons**: Developers can define multiple browsers/variants in
  a `browser.config.hjson` file and pass it via `--browser-config`. Crossbench
  natively interleaves the runs and outputs a comparative JSON report. Note:
  `--browser` and `--browser-config` are mutually exclusive in the CLI.
- **Startup & Instant Events in SQL**: Avoid using `paint.ts - min(ts)` in
  `trace_processor` SQL queries to measure startup latency, as `min(ts)`
  reflects when tracing started rather than browser startup. Instead, prefer
  recording Chromium UMA histograms via the `org.chromium.histogram_sample`
  Perfetto data source and querying `chrome_histogram_sample.sample` from
  `slice` + `args` (as done in
  `third_party/crossbench/config/benchmark/browser_startup/probe.hjson`). Only
  use timestamp deltas (`paint.ts - start.ts`) when an explicit start slice
  exists in the trace rather than querying the `dur` column on instant events
  (which have `dur = 0`).

### 5. `browser-startup` A/B Benchmarking Best Practices

When running `browser-startup` locally to compare browser binaries or feature
flags:

- **Verify Release Build Settings**: Before benchmarking, check `gn args` in the
  target output directory to ensure `is_debug = false` and
  `dcheck_always_on = false`. Debug or DCHECK-enabled builds add significant
  variance and overhead to startup paths.
- **Isolate Stories with `--separate`**: By default, multi-story configs
  (`story.hjson` defines `newtab_startup`, `blank_startup`, and
  `google_startup`) run in a single browser session unless separated. Always
  pass `--separate` so Crossbench launches a fresh browser process for each
  story, or filter to a single story with `--stories=newtab_startup`.
- **Bypass Local Environment Checks**: Pass `--env-validation=warn` when running
  on local workstations or CloudTop VMs so Crossbench does not abort on running
  background processes or CPU governors.
- **Account for Startup Bimodality**: Local startup paint metrics exhibit a
  bimodal distribution (`~9%` relative stddev). Run `--repeat=30` to `100` and
  compare medians, Hodges-Lehmann estimators, or paired per-repetition diffs
  matched by `cb_run` across interleaved runs rather than relying on small-N
  means.
- **Default ChromeDriver Isolation**: Note that ChromeDriver automatically
  passes `--disable-extensions`, `--disable-sync`, and creates a fresh temporary
  `--user-data-dir` per run by default.

### 6. Chromium Infrastructure Integration (Shard Maps)

When a Crossbench benchmark is added or modified in Chromium's
`tools/perf/core/bot_platforms.py` or its `schedule` CSV files, the changes will
not reach the swarming bots automatically.

- **Required Action**: You must regenerate the JSON shard maps by running
  `vpython3 tools/perf/core/generate_perf_sharding.py update -o` from the
  Chromium root. The resulting JSON changes in `tools/perf/core/shard_maps/`
  must be included in the CL.

### 7. Pinpoint & Trybot Executions

- **Restricted Bots**: Some performance bots (e.g., `mac-m1-pro-perf`) are
  CI-only and will return a "No props found" or "Bot invalid" error if used for
  on-demand Pinpoint jobs. Advise users to test on equivalent capacity bots like
  `mac-m1_mini_2020-perf` or standard trybots like `linux-perf-rel`.

### 8. Common Trace Processor Gotchas

- **Missing Trace Events (Empty CSVs)**: If a `trace_processor` SQL query
  returns empty results, the event was likely dropped by Perfetto. Verify that
  the event's category (e.g., `startup`) is explicitly listed in the
  `enabled_categories` of the `perfetto` probe config.
- **Dashboard Story Merging**: The Chromium `crossbench_result_converter.py`
  script ingests the aggregated `cb.results.json` file. Because Crossbench's
  `MetricsMerger` strips `cb_story` metadata for generic `trace_processor`
  queries, multiple stories will collapse into a single "Default" bucket on the
  Skia Perf dashboard. To separate them, either split the tests into separate
  benchmark registrations in `bot_platforms.py` or write a custom converter in
  `chromium/src`.
