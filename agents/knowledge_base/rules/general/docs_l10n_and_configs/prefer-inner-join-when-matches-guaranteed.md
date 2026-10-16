---
name: general/prefer-inner-join-when-matches-guaranteed
rubric: correctness
severity: should_fix
---

**Rule.** In SQL queries and PerfettoSQL modules, use `LEFT JOIN` instead of an
inner `JOIN` when joining on nullable or optional foreign keys (such as
`browser_utid`) or when chaining joins against tables that were already
left-joined, to avoid unintentionally dropping base rows. Conversely, prefer an
inner `JOIN` over `LEFT JOIN` when matching rows are guaranteed to exist or when
subsequent `WHERE` clauses discard unmatched rows.

**Why.** An inner JOIN on a NULL foreign key or on a column from an already
left-joined optional table evaluates to false when the entity is missing,
silently filtering out valid base rows. Conversely, using LEFT JOIN when matches
are guaranteed or filtered by WHERE clauses obscures schema invariants and
misleadingly implies unmatched rows can occur.

## Examples

Bad:

```cpp
-- Bad: inner JOIN on nullable foreign key silently drops rows with NULL browser_utid
FROM chrome_scroll_update_info info
JOIN thread ON info.browser_utid = thread.utid

-- Bad: inner JOIN chained on a left-joined table drops base rows when optional entity is missing
LEFT JOIN thread AS browser_main_thread
  ON browser_utid = browser_main_thread.utid
JOIN process AS browser_process
  ON browser_process.upid = browser_main_thread.upid

-- Bad: LEFT JOIN when WHERE clause or foreign key guarantees a match
SELECT SUM(delta_y)
FROM chrome_scroll_input_offsets AS input
LEFT JOIN chrome_scroll_update_info AS update_info
  ON input.scroll_update_id = update_info.id
WHERE update_info.frame_display_id = info.frame_display_id;
```

Good:

```cpp
-- Good: LEFT JOIN preserves base rows when foreign key or chained entity is optional/nullable
FROM chrome_scroll_update_input_pipeline AS input
LEFT JOIN thread AS browser_main_thread
  ON browser_utid = browser_main_thread.utid
LEFT JOIN process AS browser_process
  ON browser_process.upid = browser_main_thread.upid

-- Good: inner JOIN reflects guaranteed match and WHERE filter
SELECT SUM(delta_y)
FROM chrome_scroll_input_offsets AS input
JOIN chrome_scroll_update_info AS update_info
  ON input.scroll_update_id = update_info.id
WHERE update_info.frame_display_id = info.frame_display_id;
```
