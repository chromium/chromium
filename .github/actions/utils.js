
const http = require("http");
const https = require("https");
const { spawnSync } = require("child_process");

function getLatestRevision() {
  return process.env.GITHUB_SHA.substring(0, 12);
}

function sendBuildTestRequest(contents) {
  const text = JSON.stringify(contents);

  const headers = {
    "Content-Type": "application/json",
    "Content-Length": text.length,
    Authorization: process.env.BUILD_TEST_AUTHORIZATION,
  };

  // Allow overriding the build/test connection info for testing.
  const options = {
    hostname: process.env.BUILD_TEST_HOSTNAME || "build-test.replay.io",
    port: process.env.BUILD_TEST_PORT || 443,
    path: "/",
    method: "POST",
    headers,
  };

  const request = (process.env.BUILD_TEST_INSECURE ? http : https).request(
    options,
    response => {
      console.log(`RequestFinished Code ${response.statusCode}`);
      process.exit(response.statusCode == 200 ? 0 : 1);
    }
  );
  request.on("error", e => {
    throw new Error(`Error contacting build/test server: ${e}`);
  });
  request.write(text);
  request.end();

  setTimeout(() => {
    console.log("Timed out waiting for build/test server response");
    process.exit(1);
  }, 30000);
}

function spawnChecked(cmd, args, options) {
  const prettyCmd = [cmd].concat(args).join(" ");
  console.error(prettyCmd);

  const rv = spawnSync(cmd, args, options);

  if (rv.status != 0 || rv.error) {
    console.error(rv.error);
    throw new Error(`Spawned process failed with exit code ${rv.status}`);
  }

  return rv;
}

let gNextTaskId = 1;

function newTask(name, task, platform, dependencies = []) {
  const id = gNextTaskId++;
  return {
    id,
    name,
    task,
    platform,
    dependencies: dependencies.map(task => task.id),
  };
}

module.exports = {
  getLatestRevision,
  sendBuildTestRequest,
  spawnChecked,
  newTask,
};
