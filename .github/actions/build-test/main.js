
const {
  getLatestRevision,
  sendBuildTestRequest,
  newTask,
} = require("../utils");

const revision = getLatestRevision();

sendBuildTestRequest({
  name: `Chromium Build/Test ${revision}`,
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
      revision,
    },
    platform
  )

  const testStaticTask = newTask(
    `Chromium Static Tests ${platform}`,
    {
      kind: "StaticLiveTests",
      runtime: "chromium",
      revision,
    },
    platform,
    [buildTask]
  );

  const testPlaywrightTask = newTask(
    `Chromium Playwright Tests ${platform}`,
    {
      kind: "PlaywrightLiveTests",
      runtime: "chromium",
      revision,
    },
    platform,
    [buildTask]
  );

  return [buildTask, testStaticTask, testPlaywrightTask];
}
