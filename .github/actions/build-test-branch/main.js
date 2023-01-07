
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

const clobberInput = process.env.INPUT_CLOBBER;
console.log("Clobber", clobberInput);
const clobber = clobberInput == "true";

const slotInput = process.env.INPUT_SLOT;
console.log("Slot", slotInput);
const slot = slotInput ? +slotInput : undefined;

const runTestsInput = process.env.INPUT_RUN_TESTS;
console.log("RunTests", runTestsInput);
const runTests = runTestsInput == "true";

const numTestsInput = process.env.INPUT_NUM_TESTS;
console.log("NumTests", numTestsInput);
const numTests = numTestsInput ? +numTestsInput : undefined;

const cypressTestsInput = process.env.INPUT_CYPRESS_TESTS;
console.log("CypressTests", cypressTestsInput);
const runCypressTests = cypressTestsInput == "true";

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
        numTests,
      },
      platform,
      [buildTask]
    );
    tasks.push(testStaticTask);
  }

  if (runCypressTests) {
    const testCypressTask = newTask(
      `Chromium Cypress Tests ${platform}`,
      {
        kind: "CypressTests",
        runtime: "chromium",
        revision: chromiumRevision,
        driverRevision,
      },
      platform,
      [buildTask]
    );
    tasks.push(testCypressTask);
  }

  return tasks;
}
