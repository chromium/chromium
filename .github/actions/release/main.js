
const {
  getLatestRevision,
  sendBuildTestRequest,
  newTask,
} = require("../utils");

const revision = getLatestRevision();

sendBuildTestRequest({
  name: `Chromium Release ${revision}`,
  tasks: [
    ...platformTasks("linux"),
    ...platformTasks("macOS"),
  ],
});

function platformTasks(platform) {
  const releaseTask = newTask(
    `Release Chromium ${platform}`,
    {
      kind: "ReleaseRuntime",
      runtime: "chromium",
      revision,
    },
    platform
  );
  return [releaseTask];
}
