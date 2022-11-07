
const {
  getLatestRevision,
  sendBuildTestRequest,
  newTask,
} = require("../utils");

const branchName = process.env.GITHUB_REF_NAME;
console.log("BranchName", branchName);

const chromiumRevision = getLatestRevision();

const driverRevision = process.env.INPUT_DRIVER_REVISION;
console.log("DriverRevision", driverRevision);

const chromiumV8Revision = process.env.INPUT_V8_REVISION;
console.log("V8Revision", chromiumV8Revision);

const chromiumSkiaRevision = process.env.INPUT_SKIA_REVISION;
console.log("SkiaRevision", chromiumSkiaRevision);

const clobberInput = process.env.INPUT_CLOBBER;
console.log("Clobber", clobberInput);
const clobber = clobberInput == "true";

const slotInput = process.env.INPUT_SLOT;
console.log("Slot", slotInput);
const slot = slotInput ? +slotInput : undefined;

const runTestsInput = process.env.INPUT_RUN_TESTS;
console.log("RunTests", runTestsInput);
const runTests = runTestsInput == "true";

let requestName = `Chromium Build/Test Branch ${branchName} ${chromiumRevision}`;
if (driverRevision) {
  requestName += ` driver ${driverRevision}`;
}
if (slot) {
  requestName += ` slot ${slot}`;
}

sendBuildTestRequest({
  name: requestName,
  tasks: [
    ...platformTasks("linux"),
  ],
});

function platformTasks(platform) {
  const buildTask = newTask(
    `Build Chromium ${platform}`,
    {
      kind: "BuildRuntime",
      runtime: "chromium",
      revision: chromiumRevision,
      branch: branchName,
      branchSlot: slot,
      driverRevision,
      chromiumV8Revision,
      chromiumSkiaRevision,
      clobber,
    },
    platform
  );

  const tasks = [buildTask];

  if (runTests) {
    const testStaticTask = newTask(
      `Chromium Static Tests ${platform}`,
      {
        kind: "StaticLiveTests",
        runtime: "chromium",
        revision: chromiumRevision,
        driverRevision,
      },
      platform,
      [buildTask]
    );
    tasks.push(testStaticTask);
  }

  return tasks;
}
