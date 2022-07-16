# Overview
Diagnostic Processor Support Library (DPSL) is a collection of telemetry and
diagnostics interfaces exposed to third-parties:
   - dpsl.diagnostics (old: chromeos.diagnostics)
    | Diagnostics interface for running device diagnostics routines (tests).
   - dpsl.telemetry (old: chromeos.telemetry)
    | Telemetry (a.k.a. Probe) interface for getting device telemetry
    | information.
  - dpsl.system_events (old: chromeos.telemetry.addEventListener(..))
    | System Events interface for (un)subscribing to system events.

# Important note
The namespace for DPSL APIs is changed from chromeos.* to dpsl.*. New
third-party code should use the new namespace.

# Usage examples

## Telemetry example
```
function fetchCpuInfo() {
  dpsl.telemetry.getCpuInfo().then((cpu) => {
    console.log('CPU Architecture:', cpu.architecture);
    console.log('CPU Total # of threads:', cpu.numTotalThreads);
    let physicalCpus = cpu.physicalCpus;
    // do something
  }).catch((error) => {
    // check error message
    console.error(error.message);
  });
}
// fetch CpuInfo after two seconds.
setTimeout(fetchCpuInfo, 2000);
```

## Diagnostics example
```
// Run CPU stress routine...

function isFinalStatus(routineStatus) {
  return !(['ready', 'running', 'waiting'].includes(routineStatus.status));
}

function checkRoutineStatus(routine, routineStatus) {
  console.log('Routine Progress:', routineStatus.progressPercent);
  if (!isFinalStatus(routineStatus)) {
    setTimeout(() => {
      routine.getStatus().
       then((status) => checkRoutineStatus(routine, status)).
       catch((error) => {
         // do something with the error
         routine.stop();
       });
    }, 200);
    return;
  }
  // do something with the result…

  // do not forget to stop the routine when finished
  routine.stop();
}

function handleCpuRoutine(routine) {
  /** @type {!Promise<RoutineStatus>} */
  routine.getStatus().then((status) => checkRoutineStatus(routine, status));
}

/** @type {Promise<Array<string>>} */
dpsl.diagnostics.getAvailableRoutines().then((routineList) => {
  if (!routineList.includes('cpu-stress')) return;

  /** @type {Promise<Routine>} */
  dpsl.diagnostics.cpu.runStressRoutine({length_seconds: 2})
    .then(handleCpuRoutine)
    .catch((error) => {
      console.error('Couldn’t run routine: ', error.message);
  });
});
```

## System events example
```
// (un)subscribe on system_events

function osSuspendCallback() { … }

if (dpsl.system_events.getAvailableEvents().includes('os-suspend')) {
  dpsl.system_events.power.addOnOsSuspendListener(osSuspendCallback);
  ..
  dpsl.system_events.power.removeOnOsSuspendListener(osSuspendCallback);
}
```
