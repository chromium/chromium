---
name: general/prefer-group-by-aggregation-over-correlated-subqueries
rubric: safety
severity: should_fix
---

**Rule.** In SQL queries and PerfettoSQL stdlib modules, prefer using a LEFT
JOIN (or JOIN) combined with a GROUP BY clause rather than correlated scalar
subqueries in SELECT projections when aggregating data from a related table. Use
aggregate functions (such as GROUP_CONCAT, COUNT, SUM, MIN, MAX) over the joined
relation.

**Why.** Evaluating a correlated scalar subquery in the SELECT projection list
forces the query engine to execute the subquery row-by-row for each row emitted
by the outer query (N+1 overhead). A LEFT JOIN with GROUP BY processes the
relations set-wise, allowing the engine to perform aggregation in a single pass.

## Examples

Bad:

```cpp
-- Correlated scalar subquery executed row-by-row for each outer row:
WITH tagged_frames AS (
  SELECT
    frame.id AS frame_id,
    (
      SELECT GROUP_CONCAT(tag ORDER BY tag)
      FROM chrome_scroll_jank_tags
      WHERE frame_id = frame.id
    ) AS tags
  FROM chrome_scroll_frame_info AS frame
  WHERE frame.is_janky
)
```

Good:

```cpp
-- Set-wise LEFT JOIN with GROUP BY aggregation in a single pass:
WITH tagged_frames AS (
  SELECT
    frame.id AS frame_id,
    GROUP_CONCAT(tag ORDER BY tag) AS tags
  FROM chrome_scroll_frame_info AS frame
  LEFT JOIN chrome_scroll_jank_tags tag ON tag.frame_id = frame.id
  WHERE frame.is_janky
  GROUP BY frame_id
)
```
