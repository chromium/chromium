
const {
  getLatestRevision,
  sendBuildTestRequest,
  newTask,
} = require("../utils");

const branchName = getBranchName(process.env.GITHUB_REF);
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

  return [buildTask, testStaticTask];
}

function getBranchName(refName) {
  // Strip everything after the last "/" from the ref to get the branch name.
  const index = refName.lastIndexOf("/");
  if (index == -1) {
    return refName;
  }
  return refName.substring(index + 1);
}
