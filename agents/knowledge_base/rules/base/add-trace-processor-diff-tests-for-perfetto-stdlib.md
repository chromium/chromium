---
name: base/add-trace-processor-diff-tests-for-perfetto-stdlib
rubric: testing
severity: should_fix
---

**Rule.** When introducing or modifying PerfettoSQL tables, views, functions, or
metrics in the Chrome standard library (`base/tracing/stdlib/chrome/`), add
corresponding Trace Processor diff tests under
`base/tracing/test/trace_processor/diff_tests/chrome/` to verify query output
against realistic test traces. If creating a new `TestSuite` class in a new test
file, register it in
`base/tracing/test/trace_processor/diff_tests/include_index.py`.

**Why.** PerfettoSQL definitions for Chrome rely on complex SQL joins and trace
event slices that must remain correct and backwards compatible across older and
newer trace formats. Diff tests validate standard library queries against fixed
trace recordings to prevent query regressions and schema breakages.

## Examples

Bad:

```cpp
# In base/tracing/stdlib/chrome/scroll_jank.sql:
# CREATE PERFETTO TABLE chrome_scroll_frame_info_v4 AS ...
#
# (CL modifies or introduces PerfettoSQL definitions without adding or updating
# corresponding diff tests under base/tracing/test/trace_processor/diff_tests/chrome/)
```

Good:

```cpp
# In base/tracing/test/trace_processor/diff_tests/chrome/tests_scroll_jank.py:
def test_chrome_scroll_frame_info_v4(self):
  return DiffTestBlueprint(
      trace=DataPath('scroll_m144.pftrace'),
      query="""
      INCLUDE PERFETTO MODULE chrome.scroll_jank_v4;
      SELECT * FROM chrome_scroll_frame_info_v4;
      """,
      out=Csv("""..."""))
```
