SELECT IMPORT('chrome.speedometer');

SELECT
  iteration,
  ts,
  dur,
  total,
  format('%.1f', mean) AS mean,
  format('%.1f', geomean) AS geomean,
  format('%.1f', score) AS score,
  num_measurements
FROM
  chrome_speedometer_iteration,
  (
    SELECT iteration, COUNT(*) AS num_measurements
    FROM chrome_speedometer_measure
    GROUP BY iteration
  )
USING (iteration)
ORDER BY iteration;
