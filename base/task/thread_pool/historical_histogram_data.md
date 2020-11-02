# Historical Histogram Data

This page presents data captured from `base::ThreadPool` histograms at a given
point in time so it can be used in future design decisions.

All data is 28-day aggregation on Stable channel.

## Number of tasks between waits

Number of tasks between two waits by a foreground worker thread in a
browser/renderer process.

Histogram name: ThreadPool.NumTasksBetweenWaits.(Browser/Renderer).Foreground
Date: August 2019
Values in tables below are percentiles.

### Windows

| Number of tasks | Browser process | Renderer process |
|-----------------|-----------------|------------------|
| 1               | 87              | 92               |
| 2               | 95              | 98               |
| 5               | 99              | 100              |

### Mac

| Number of tasks | Browser process | Renderer process |
|-----------------|-----------------|------------------|
| 1               | 81              | 90               |
| 2               | 92              | 97               |
| 5               | 98              | 100              |

### Android

| Number of tasks | Browser process | Renderer process |
|-----------------|-----------------|------------------|
| 1               | 92              | 96               |
| 2               | 97              | 98               |
| 5               | 99              | 100              |


## Number of tasks run while queueing

Number of tasks run by ThreadPool while task was queuing (from time task was
posted until time it was run). Recorded for dummy heartbeat tasks in the
*browser* process. The heartbeat recording avoids dependencies between this
report and other work in the system.

Histogram name: ThreadPool.NumTasksRunWhileQueuing.Browser.*
Date: September 2019
Values in tables below are percentiles.

Note: In *renderer* processes, on all platforms/priorities, 0 tasks are run
while queuing at 99.5th percentile.

### Windows

| Number of tasks | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|-----------------|---------------|--------------|-------------|
| 0               | 95            | 93           | 90          |
| 1               | 98            | 95           | 92          |
| 2               | 99            | 96           | 93          |
| 5               | 100           | 98           | 95          |

### Mac

| Number of tasks | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|-----------------|---------------|--------------|-------------|
| 0               | 100           | 100          | 99          |
| 1               | 100           | 100          | 99          |
| 2               | 100           | 100          | 99          |
| 5               | 100           | 100          | 100         |

### Android

| Number of tasks | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|-----------------|---------------|--------------|-------------|
| 0               | 99            | 98           | 97          |
| 1               | 100           | 99           | 99          |
| 2               | 100           | 99           | 99          |
| 5               | 100           | 100          | 100         |

### Chrome OS

For all priorities, 0 tasks are run while queueing at 99.5th percentile.

### Analysis

The number of tasks that run while a BEST_EFFORT task is queued is unexpectedly
low. We should explore creating threads less aggressively, at the expense of
keeping BEST_EFFORT tasks in the queue for a longer time. See
[Bug 906079](https://crbug.com/906079).

## Number of workers

Number of workers that live in a given SchedulerWorkerPool. Recorded every
59 minutes.

Histogram name: ThreadPool.NumWorkers.(Browser/Renderer/ContentChild).Foreground
Date: July 2020
Values in tables below are percentiles.

### Windows

| Number of workers | Browser process | Renderer process |
|-------------------|-----------------|------------------|
| 2                 | 39              | 87               |
| 5                 | 88              | 98               |
| 10                | 98              | 100              |

### Mac

| Number of workers | Browser process | Renderer process |
|-------------------|-----------------|------------------|
| 2                 | 52              | 98               |
| 5                 | 94              | 99               |
| 10                | 99              | 100              |

### Android

| Number of workers | Browser process | Renderer process |
|-------------------|-----------------|------------------|
| 2                 | 30              | 84               |
| 5                 | 80              | 89               |
| 10                | 99              | 100              |

## Number of active workers

Number of workers running a task in a given SchedulerWorkerPool. Recorded
every 59 minutes

Histogram name: ThreadPool.NumActiveWorkers.(Browser/Renderer/ContentChild).Foreground
Date: July 2020
Values in tables below are percentiles.

### Windows

| Number of workers | Browser process | Renderer process |
|-------------------|-----------------|------------------|
| 0                 | 88.72           | 99.72            |
| 1                 | 90.36           | 99.89            |
| 2                 | 92.33           | 99.95            |
| 5                 | 97.63           | 99.99            |

### Mac

| Number of workers | Browser process | Renderer process |
|-------------------|-----------------|------------------|
| 0                 | 99.21           | 99.92            |
| 1                 | 99.85           | 99.98            |
| 2                 | 99.94           | 99.99            |
| 5                 | 99.99           | 100              |

### Android

| Number of workers | Browser process | Renderer process |
|-------------------|-----------------|------------------|
| 0                 | 94.18           | 99.59            |
| 1                 | 95.66           | 99.9             |
| 2                 | 97.85           | 99.93            |
| 5                 | 99.50           | 99.97            |

## Detach Duration

Time elapsed between when a shared worker is detached and when a new shared
worker is created. Recorded each time a shared worker is created.

Histogram name: ThreadPool.DetachDuration.(Browser/Renderer).Foreground
Date: October 2020

### Windows

| Percentile  | Browser process (seconds) | Renderer process (seconds) |
|-------------|---------------------------|----------------------------|
| 25          | 22                        | 22                         |
| 50          | 80                        | 65                         |
| 75          | 301                       | 209                        |
| 95          | 3468                      | 1801                       |

### Mac

| Percentile  | Browser process (seconds) | Renderer process (seconds) |
|-------------|---------------------------|----------------------------|
| 25          | 23                        | 20                         |
| 50          | 82                        | 57                         |
| 75          | 285                       | 209                        |
| 95          | 2720                      | 1665                       |

### Android

| Percentile  | Browser process (seconds) | Renderer process (seconds) |
|-------------|---------------------------|----------------------------|
| 25          | 96                        | 97                         |
| 50          | 362                       | 418                        |
| 75          | 1161                      | 1307                       |
| 95          | > 1 hour                  | > 1 hour                   |

## Task Latency

Time elapsed between when a task is posted and when it starts to run. Recorded
for each task that runs inside the ThreadPool.

Unit: microseconds

Histogram name: ThreadPool.TaskLatencyMicroseconds.(Browser/Renderer).(UserBlocking/UserVisible/Background)TaskPriority
Date: October 13, 2020

### Windows

#### Browser process

| Percentile | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|------------|---------------|--------------|-------------|
| 25         | 11            | 13           | 39          |
| 50         | 19            | 27           | 386         |
| 75         | 44            | 60           | 2533        |
| 95         | 495           | 701          | > 20 ms     |
| 99         | > 20 ms       | 16705        | > 20 ms     |

Count distribution:
- USER_BLOCKING: 14%
- USER_VISIBLE: 72%
- BEST_EFFORT: 14%

#### Renderer process

| Percentile | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|------------|---------------|--------------|-------------|
| 25         | 14            | 18           | 151         |
| 50         | 30            | 39           | 567         |
| 75         | 65            | 86           | 9145        |
| 95         | 1102          | 2230         | > 20 ms     |
| 99         | 7664          | > 20 ms      | > 20 ms     |

Count distribution:
- USER_BLOCKING: 45%
- USER_VISIBLE: 54%
- BEST_EFFORT: 0%

### Mac

#### Browser process

| Percentile | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|------------|---------------|--------------|-------------|
| 25         | 15            | 15           | 1748        |
| 50         | 28            | 32           | 4848        |
| 75         | 99            | 90           | 12492       |
| 95         | 2101          | 1781         | > 20 ms     |
| 99         | 13129         | 17192        | > 20 ms     |

Count distribution:
- USER_BLOCKING: 13%
- USER_VISIBLE: 81%
- BEST_EFFORT: 6%

#### Renderer process

| Percentile | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|------------|---------------|--------------|-------------|
| 25         | 15            | 22           | 108         |
| 50         | 28            | 36           | 175         |
| 75         | 66            | 58           | 1783        |
| 95         | 630           | 684          | > 20 ms     |
| 99         | 3895          | 6966         | > 20 ms     |

Count distribution:
- USER_BLOCKING: 45%
- USER_VISIBLE: 55%
- BEST_EFFORT: 0%

### Chrome OS

#### Browser process

| Percentile | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|------------|---------------|--------------|-------------|
| 25         | 38            | 23           | 17          |
| 50         | 68            | 65           | 28          |
| 75         | 218           | 198          | 613         |
| 95         | 2293          | 3615         | > 20 ms     |
| 99         | 18084         | > 20 ms      | > 20 ms     |

Count distribution:
- USER_BLOCKING: 7%
- USER_VISIBLE: 40%
- BEST_EFFORT: 53%

#### Renderer process

| Percentile | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|------------|---------------|--------------|-------------|
| 25         | 40            | 48           | 74          |
| 50         | 111           | 108          | 484         |
| 75         | 496           | 628          | > 20 ms     |
| 95         | 5394          | 5866         | > 20 ms     |
| 99         | > 20 ms       | > 20 ms      | > 20 ms     |

Count distribution:
- USER_BLOCKING: 68%
- USER_VISIBLE: 32%
- BEST_EFFORT: 0%

### Android

#### Browser process

| Percentile | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|------------|---------------|--------------|-------------|
| 25         | 43            | 37           | 53          |
| 50         | 86            | 103          | 122         |
| 75         | 224           | 412          | 648         |
| 95         | 1719          | > 20 ms      | > 20 ms     |
| 99         | 10001         | > 20 ms      | > 20 ms     |

Count distribution:
- USER_BLOCKING: 58%
- USER_VISIBLE: 39%
- BEST_EFFORT: 4%

#### Renderer process

| Percentile | USER_BLOCKING | USER_VISIBLE | BEST_EFFORT |
|------------|---------------|--------------|-------------|
| 25         | 45            | 63           | 72          |
| 50         | 90            | 109          | 115         |
| 75         | 242           | 246          | 235         |
| 95         | 1635          | 960          | 829         |
| 99         | 6728          | 3273         | 3273        |

Count distribution:
- USER_BLOCKING: 35%
- USER_VISIBLE: 63%
- BEST_EFFORT: 2%
