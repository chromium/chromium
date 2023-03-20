
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
    //...platformTasks("macOS"),
    //...platformTasks("windows"),
  ],
});

function platformTasks(platform) {
  const tasks = [];

  const buildTask = newTask(
    `Build Chromium ${platform}`,
    {
      kind: "BuildRuntime",
      runtime: "chromium",
      revision,
    },
    platform
  );
  tasks.push(buildTask);

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
  tasks.push(testStaticTask);

  // Playwright tests are currently only supported on linux.
  if (process.platform == "linux") {
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
    tasks.push(testPlaywrightTask);
  }

  return tasks;
}
