# End To End Tests for Metrics

[TOC]

## Background
Chrome's [speed metrics][csm] are reported to a number of downstream consumers:

- Web Performance APIs (typically through the PerformanceObserver)
- UKM (as seen in `chrome://ukm`]
- UMA (as seen in `chrome://histograms`)
- Trace Events (as seen in `chrome://tracing`)

Due to the diverse use cases for and contexts required by each consumer, we
can't always guarantee that the calculation of each metric is done entirely in
one place.

Further, some consumers _must_ observe distinct values (e.g. privacy
and security requirements can mean that a Performance API must see a value
based solely on first-party content while a more holistic value can be reported
internally).

Because of this, it's all too easy to introduce bugs where
consumers see incorrect and/or inconsistent values. This is "a bad thing"™ that
we'd like to avoid, so we write integration tests to assert each consumer sees
the correct value.

## Metrics Integration Test Framework
To make it easier to write tests of metrics emissions, we have the Metrics
Integration Test Framework. The framework makes it easy to

- Run an integration test
    - Load a web page consisting of a given string literal
    - Serve resources that can be fetched by the above page
- Record and verify reports of metrics as they're emitted
    - Performance APIs can be queryed in-page and tested with familiar
      testharness.js assertions
    - UMA metrics can be observed and inspected with a HistogramTester
    - UKM metrics can be observed and inspected with a TestAutoSetUkmRecorder
    - Trace Events can be queried and aggregated with a TraceAnalyzer

## Examples
See the [source](metric_browsertest.cc)!

## Tips and Tricks
Use [`content::EvalJS`][evaljs] to pass JavaScript values back to C++ and check for
consistency.

Use [`xvfb-run`][xvfb-run] when running the `browser_test` executable.

- no more flashing windows
- no chance to accidentally send real input to the test

[csm]: https://docs.google.com/document/d/1Ww487ZskJ-xBmJGwPO-XPz_QcJvw-kSNffm0nPhVpj8
[evaljs]: /content/public/test/browser_test_utils.h
[xvfb-run]: https://manpages.debian.org/testing/xvfb/xvfb-run.1.en.html
