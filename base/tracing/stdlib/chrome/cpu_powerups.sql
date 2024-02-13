-- Copyright 2023 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Find causes for CPUs powering up.
--
-- The scripts below analyse traces with the following tracing options
-- enabled:
--
--  - Linux kernel:
---    "power/*", "sched/*", "task/*",
--  - Chromium:
--      "toplevel", "toplevel.flow".

-- Noteworthy tables:
--
--   chrome_cpu_power_first_toplevel_slice_after_powerup :: The top-level
--      slices that ran after a CPU power-up.

-- The CPU power transitions in the trace.
-- Power states are encoded as non-negative integers, with zero representing
-- full-power operation and positive values representing increasingly deep
-- sleep states.
--
-- On ARM systems, power state 1 represents the WFI (Wait For Interrupt) sleep
-- state that the CPU enters while idle.
CREATE PERFETTO VIEW chrome_cpu_power_slice(
  -- The timestamp at the start of the slice.
  ts INT,
  -- The duration of the slice.
  dur INT,
  -- The CPU on which the transition occurred
  cpu INT,
  -- The power state that the CPU was in at time 'ts' for duration 'dur'.
  power_state INT,
  -- The power state that the CPU was previously in.
  previous_power_state INT,
  -- A unique ID for the CPU power-up.
  powerup_id INT
) AS
  WITH cpu_power_states AS (
    SELECT
      c.id AS id,
      cct.cpu AS cpu,
      c.ts,
      -- Encode the 'value' field as a power state.
      CAST((CASE c.value WHEN 4294967295 THEN 0 ELSE c.value + 1 END)
        AS INT) AS power_state
    FROM counter AS c
    JOIN cpu_counter_track AS cct
      ON c.track_id = cct.id
    WHERE cct.name = 'cpuidle'
  )
  SELECT *
  FROM (
    SELECT
      ts,
      LEAD(ts) OVER (PARTITION BY cpu ORDER BY ts ASC) - ts
        AS dur,
      cpu,
      power_state,
      LAG(power_state) OVER (PARTITION BY cpu ORDER BY ts ASC)
        AS previous_power_state,
      id AS powerup_id
    FROM cpu_power_states
  )
  WHERE dur IS NOT NULL
    AND previous_power_state IS NOT NULL
    AND power_state = 0                      -- Track full-power states.
    AND power_state != previous_power_state  -- Skip missing spans.
    ORDER BY ts ASC;

-- We do not want scheduler slices with utid = 0 (the 'swapper' kernel thread).
CREATE PERFETTO VIEW _cpu_power_valid_sched_slice AS
  SELECT *
  FROM sched_slice
  WHERE utid != 0;

-- Join scheduler slices with the spans with CPU power slices.
--
-- There multiple scheduler slices could fall into one CPU power slice.
--
---  CPU Power:
--   |----------------------------|....................|---------|
--   A       <cpu active>         B     <cpu idling>   C         D

--   Scheduler slices on that CPU:
--     |-----T1-----| |....T2....|                      |---T3--|
--     E            F G          H                      I       J
--
-- Here threads T1 and T2 executed in CPU power slice [A,B].  The
-- time between F and G represents time between threads in the kernel.
CREATE VIRTUAL TABLE _cpu_power_and_sched_slice
USING
  SPAN_JOIN(chrome_cpu_power_slice PARTITIONED cpu,
            _cpu_power_valid_sched_slice PARTITIONED cpu);

-- The Linux scheduler slices that executed immediately after a
-- CPU power up.
CREATE PERFETTO TABLE chrome_cpu_power_first_sched_slice_after_powerup(
  -- The timestamp at the start of the slice.
  ts INT,
  -- The duration of the slice.
  dur INT,
  -- The cpu on which the slice executed.
  cpu INT,
  -- Id for the sched_slice table.
  sched_id INT,
  -- Unique id for the thread that ran within the slice.
  utid INT,
  -- The CPU's power state before this slice.
  previous_power_state INT,
  -- A unique ID for the CPU power-up.
  powerup_id INT
) AS
SELECT
  ts,
  dur,
  cpu,
  id AS sched_id,
  utid,
  previous_power_state,
  powerup_id
FROM _cpu_power_and_sched_slice
WHERE power_state = 0     -- Power-ups only.
GROUP BY cpu, powerup_id
HAVING ts = MIN(ts)       -- There will only be one MIN sched slice
                          -- per CPU power up.
ORDER BY ts ASC;

-- A view joining thread tracks and top-level slices.
--
-- This view is intended to be intersected by time with the scheduler
-- slices scheduled after a CPU power up.
--
--   utid      Thread unique id.
--   slice_id  The slice_id for the top-level slice.
--   ts        Starting timestamp for the slice.
--   dur       The duration for the slice.
CREATE PERFETTO VIEW _cpu_power_thread_and_toplevel_slice AS
  SELECT
    t.utid AS utid,
    s.id AS slice_id,
    s.ts,
    s.dur
  FROM slice AS s
  JOIN thread_track AS t
    ON s.track_id = t.id
  WHERE s.depth = 0   -- Top-level slices only.
  ORDER BY ts ASC;

-- A table holding the slices that executed within the scheduler
-- slice that ran on a CPU immediately after power-up.
--
-- @column  ts        Timestamp of the resulting slice
-- @column dur        Duration of the slice.
-- @column cpu        The CPU the sched slice ran on.
-- @column utid       Unique thread id for the slice.
-- @column sched_id   'id' field from the sched_slice table.
-- @column type       From the sched_slice table, always 'sched_slice'.
-- @column end_state  The ending state for the sched_slice
-- @column priority   The kernel thread priority
-- @column slice_id   Id of the top-level slice for this (sched) slice.
CREATE VIRTUAL TABLE chrome_cpu_power_post_powerup_slice
USING
  SPAN_JOIN(chrome_cpu_power_first_sched_slice_after_powerup PARTITIONED utid,
            _cpu_power_thread_and_toplevel_slice PARTITIONED utid);

-- The first top-level slice that ran after a CPU power-up.
CREATE PERFETTO VIEW chrome_cpu_power_first_toplevel_slice_after_powerup(
  -- ID of the slice in the slice table.
  slice_id INT,
  -- The power state of the CPU prior to power-up.
  previous_power_state INT
) AS
  SELECT slice_id, previous_power_state
  FROM chrome_cpu_power_post_powerup_slice
  GROUP BY cpu, powerup_id
  HAVING ts = MIN(ts)
  ORDER BY ts ASC;
