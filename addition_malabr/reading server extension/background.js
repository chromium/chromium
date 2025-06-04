chrome.runtime.onInstalled.addListener(() => {
  console.log('Extension installed.');
});

chrome.action.onClicked.addListener((tab) => {
  chrome.scripting.executeScript({
    target: { tabId: tab.id },
    function: fetchDataFromServer
  });
});

function fetchDataFromServer() {
  chrome.readServer.readData((response) => {
    if (response.data) {
      console.log('Data fetched from server:', response.data);
      alert('Data fetched: ' + response.data);
    } else if (response.message) {
      console.error('Error:', response.message);
      alert('Error: ' + response.message);
    }
  });
}

function sendDataToServer(dataToSend) {
  chrome.readServer.sendData(dataToSend, (response) => {
    if (response) {
      console.log('Server response:', response);
      alert('Server responded: ' + response);
    } else {
      console.error('Error sending data to server.');
      alert('Error: Failed to send data to the server.');
    }
  });
}

function uploadFileToServer() {
  chrome.readServer.uploadTrainingData((response) => {
    if (response) {
      console.log('File upload response:', response);
      alert('File uploaded successfully: ' + response);
    } else {
      console.error('Error uploading file.');
      alert('Error: Failed to upload the file.');
    }
  });
}

function trainModelOnServer() {
  chrome.readServer.trainModel((response) => {
    if (response) {
      console.log('Training response:', response);
      alert('Model training initiated: ' + response);
    } else {
      console.error('Error initiating model training.');
      alert('Error: Failed to start model training.');
    }
  });
}

function inferOnServer(features) {
  chrome.readServer.inference(features, (response) => {
    try {
      const parsedResponse = JSON.parse(response);
      if (parsedResponse.prediction) {
        console.log('Inference result:', parsedResponse.prediction);
        alert('Prediction: ' + parsedResponse.prediction);
      } else {
        console.error('Error:', parsedResponse.error);
        alert('Error: ' + parsedResponse.error);
      }
    } catch (e) {
      console.error('Failed to parse inference response:', e);
      alert('Error: Failed to parse inference response');
    }
  });
}

