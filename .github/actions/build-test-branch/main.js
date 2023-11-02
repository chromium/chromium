
const {
  getLatestRevision,
  sendBuildTestRequest,
  newTask,
} = require("../utils");

const branchName = process.env.GITHUB_REF_NAME;
console.log("BranchName", branchName);

const chromiumRevision = getLatestRevision();

const linuxBuildInput = process.env.INPUT_LINUX_BUILD;
console.log("LinuxBuild", linuxBuildInput);
const linuxBuild = linuxBuildInput == "true";

const macBuildInput = process.env.INPUT_MAC_BUILD;
console.log("MacBuild", macBuildInput);
const macBuild = macBuildInput == "true";

const windowsBuildInput = process.env.INPUT_WINDOWS_BUILD;
console.log("WindowsBuild", windowsBuildInput);
const windowsBuild = windowsBuildInput == "true";

const driverRevision = process.env.INPUT_DRIVER_REVISION;
console.log("DriverRevision", driverRevision);

const clobberInput = process.env.INPUT_CLOBBER;
console.log("Clobber", clobberInput);
const clobber = clobberInput == "true";

const slotInput = process.env.INPUT_SLOT;
console.log("Slot", slotInput);
const slot = slotInput ? +slotInput : undefined;

const numStaticTestsInput = process.env.INPUT_NUM_STATIC_TESTS;
console.log("NumStaticTests", numStaticTestsInput);
const numStaticTests = numStaticTestsInput ? +numStaticTestsInput : 30;

const runTestSuitesInput = process.env.INPUT_RUN_TEST_SUITES;
console.log("RunTestSuites", runTestSuitesInput);
const runTestSuites = runTestSuitesInput == "true";

const testSuitesFilterInput = process.env.INPUT_TEST_SUITES_FILTER;
console.log("TestSuitesFilter", testSuitesFilterInput);

const testSuiteRunsInput = process.env.INPUT_TEST_SUITE_RUNS;
console.log("TestSuiteRuns", testSuiteRunsInput);
const testSuiteRuns = testSuiteRunsInput ? +testSuiteRunsInput : undefined;

let requestName = `Chromium Build/Test Branch ${branchName} ${chromiumRevision}`;
if (driverRevision) {
  requestName += ` driver ${driverRevision}`;
}
if (slot) {
  requestName += ` slot ${slot}`;
}

const allTasks = [];
if (linuxBuild) {
  allTasks.push(...platformTasks("linux"));
}
if (macBuild) {
  allTasks.push(...platformTasks("macOS"));
}
if (windowsBuild) {
  allTasks.push(...platformTasks("windows"));
}

sendBuildTestRequest({ name: requestName, tasks: allTasks });

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

  if (platform == "macOS") {
    const buildARMTask = newTask(
      `Build Chromium ${platform} ARM`,
      {
        kind: "BuildRuntime",
        runtime: "chromium",
        revision: chromiumRevision,
        branch: branchName,
        branchSlot: slot,
        driverRevision,
        clobber,
        useARM: true,
      },
      platform
    );
    tasks.push(buildARMTask);
  }

  if (numStaticTests) {
    const testStaticTask = newTask(
      `Chromium Static Tests ${platform}`,
      {
        kind: "StaticLiveTests",
        runtime: "chromium",
        revision: chromiumRevision,
        driverRevision,
        numTests: numStaticTests,
      },
      platform,
      [buildTask]
    );
    tasks.push(testStaticTask);
  }

  if (runTestSuites) {
    const testSuitesTask = newTask(
      `Chromium Test Suites ${platform}`,
      {
        kind: "CypressTests",
        runtime: "chromium",
        revision: chromiumRevision,
        driverRevision,
        filter: testSuitesFilterInput,
        numRuns: testSuiteRuns,
      },
      platform,
      [buildTask]
    );
    tasks.push(testSuitesTask);
  }

  return tasks;
}
